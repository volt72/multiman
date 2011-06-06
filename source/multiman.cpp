#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <stddef.h>
#include <netdb.h>
#include <stdbool.h>

#include <sys/synchronization.h>
#include <sys/spu_initialize.h>
#include <sys/ppu_thread.h>
#include <sys/return_code.h>
#include <sys/sys_time.h>
#include <sys/process.h>
#include <sys/memory.h>
#include <sys/timer.h>
#include <sys/paths.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/vm.h>

#define MB(x) ((x)*1024*1024)

#include <cell/http.h>
#include <cell/mixer.h>
#include <cell/codec.h>
#include <cell/audio.h>
#include <cell/gcm.h>
#include <cell/sysmodule.h>
#include <cell/font.h>
#include <cell/fontFT.h>
#include <cell/dbgfont.h>
#include <cell/mstream.h>
#include <cell/cell_fs.h>
#include <cell/control_console.h>
#include <cell/rtc.h>
#include <cell/rtc/rtcsvc.h>

#include <cell/pad.h>
#include <cell/mouse.h>
#include <cell/keyboard.h>

#include <cell/codec/pngdec.h>
#include <cell/codec/jpgdec.h>
#include <cell/codec.h>

#include <sysutil/sysutil_video_export.h>
#include <sysutil/sysutil_music_export.h>
#include <sysutil/sysutil_photo_export.h>

#include <sysutil/sysutil_msgdialog.h>
#include <sysutil/sysutil_oskdialog.h>
#include <sysutil/sysutil_syscache.h>
#include <sysutil/sysutil_sysparam.h>
//#include <sysutil/sysutil_discgame.h>
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_screenshot.h>
#include <sysutil/sysutil_bgmplayback.h>
#include <sysutil/sysutil_webbrowser.h>

#include <netex/libnetctl.h>
#include <netex/errno.h>
#include <netex/net.h>

#include <arpa/inet.h>
#include <netinet/in.h>
//#include <np.h>
//#include <np/drm.h>


#include <libftp.h>

#if (CELL_SDK_VERSION>0x210001)
#include "libpfsm.h"
#endif

#include "mscommon.h"
#include "semaphore.h"

#include "syscall8.h"
#include "syscall36.h"

#include "graphics.h"
#include "fonts.h"
//#include "video.h"

SYS_PROCESS_PARAM(64, 0x100000)

//colors (COLOR.INI)
u32 COL_PS3DISC=0xff807000;
u32 COL_PS3DISCSEL=0xfff0e000;

u32 COL_SEL=0xff00ffff;
u32 COL_PS3=0xe0e0e0e0;
u32 COL_PS2=0xff06b02e;
u32 COL_DVD=0xffdcc503;
u32 COL_BDMV=0xff0050ff;
u32 COL_AVCHD=0xff30ffff;

u32 COL_LEGEND=0xc0c0c0a0;

u32 COL_FMFILE=0xc0c0c0c0;
u32 COL_FMDIR=0xc0808080;
u32 COL_FMJPG=0xcc00cc00;
u32 COL_FMMP3=0xc033ffee;
u32 COL_FMEXE=0xc03310ee;

u32 COL_HEXVIEW=0xc0a0a0a0;
u32 COL_SPLIT=0xc00080ff;

u32 COL_XMB_CLOCK=0xffd0d0d0;
u32 COL_XMB_COLUMN=0xf0e0e0e0;
u32 COL_XMB_TITLE=0xf0e0e0e0;
u32 COL_XMB_SUBTITLE=0xf0909090;
u8	XMB_SPARK_SIZE=4;
u32 XMB_SPARK_COLOR=0xffffff00;

int th_device_list=1;
int th_device_separator=1;
int th_device_separator_y=956;
int th_legend=1;
int th_legend_y=853;
int th_drive_icon=1;
int th_drive_icon_x=1790;
int th_drive_icon_y=964;


//NTFS
//#define MAX_PFS_VOLUMES 1
int max_usb_volumes=1;

typedef struct {
	uint32_t total;
	uint32_t avail;
} _meminfo;
_meminfo meminfo;


#if (CELL_SDK_VERSION>0x210001)
typedef struct _DEV_INFO {
    struct _DEV_INFO *next;
	PFSM_DEVICE dev;
} DEV_INFO;
static DEV_INFO *dev_info;
#endif

typedef uint64_t u64;

	CellAudioPortParam audioParam;
	CellAudioPortConfig portConfig;
	int sizeNeeded;
	int *mp3Memory;
	int _mp3_buffer=MB(24);//1920*1080*4*2;//24;- //(MB)
	u8 vm_real_size=4;
	int mp3_freq=44100;
	int mp3_durr=0;
	char mp3_now_playing[128];
	float mp3_volume=0.5f;
	static long pData, pDataB;
	char my_mp3_file[512];
	int mp3_status;
	char *stream;
	bool force_mp3;
	char force_mp3_file[512];


void *color_base_addr;
u32 frame_index = 0;
//u32 c_frame_index = 0;
u32 video_buffer;
int V_WIDTH, V_HEIGHT;//, _V_WIDTH, _V_HEIGHT;
bool use_depth=1; //for blend/alpha

int mp_WIDTH=30, mp_HEIGHT=42; //mouse icon HR

//MP3 defines
#define SAMPLE_FREQUENCY        (44100)
#define SAMPLE_CHANNELS         (2)
#define MAX_STREAMS             (2) //CELL_MS_MAX_STREAMS//(400)
#define MAX_SUBS				(4) //(31)
#define BUF_SIZE				(2 * 1024 * 1024) // for network and file_copy buffering

//folder copy
#define MAX_FAST_FILES			1
#define MAX_FAST_FILE_SIZE		(3 * 1024 * 1024) //used x3 = 9MB


#define MEMORY_CONTAINER_SIZE_WEB (64 * 1024 * 1024) //for webbrowser
#define MEMORY_CONTAINER_SIZE (5 * 1024 * 1024) //for OSK, video/photo/music export
uint32_t MEMORY_CONTAINER_SIZE_ACTIVE;

enum {
	CALLBACK_TYPE_INITIALIZE = 0,
	CALLBACK_TYPE_REGIST_1,
	CALLBACK_TYPE_FINALIZE
};
int ve_initialized=0;
int me_initialized=0;
int pe_initialized=0;
static sys_memory_container_t memory_container;
static sys_memory_container_t memory_container_web;
static int ve_result = 123;
static int me_result = 123;
static int pe_result = 123;

static void video_export( char *filename_v, char *album, int to_unregister);
static int video_finalize( void );
static void cb_export_finish( int result, void *userdata);
static void cb_export_finish2( int result, void *userdata);

static void music_export( char *filename_v, char *album, int to_unregister);
static int music_finalize( void );
static void cb_export_finish_m( int result, void *userdata);
static void cb_export_finish2_m( int result, void *userdata);

static void photo_export( char *filename_v, char *album, int to_unregister);
static int photo_finalize( void );
static void cb_export_finish_p( int result, void *userdata);
static void cb_export_finish2_p( int result, void *userdata);

int download_file(const char *http_file, const char *save_path, int show_progress);
int download_file_th(const char *http_file, const char *save_path, int params);

void replacemem(uint64_t _val_search1, uint64_t _val_replace1);

static void del_temp( char *path);
static void parse_color_ini();

void pokeq( uint64_t addr, uint64_t val);
uint64_t peekq(uint64_t addr);
void enable_sc36();

void write_last_state();
void save_options();

void cache_png(char *path, char *title_id);

void mip_texture( uint8_t *buffer_to, uint8_t *buffer_from, uint32_t width, uint32_t height, int scaleF);
void blur_texture(uint8_t *buffer_to, uint32_t width, uint32_t height, int x, int y,  int wx, int wy, uint32_t c_BRI, int use_grayscale, int iterations, int p_range);

void print_label(float x, float y, float scale, uint32_t color, char *str1p, float weight, float slant, int font);
void print_label_ex(float x, float y, float scale, uint32_t color, char *str1p, float weight, float slant, int font, float hscale, float vscale, int centered);
void flush_ttf(uint8_t *buffer, uint32_t _V_WIDTH, uint32_t _V_HEIGHT);

void file_copy(char *path, char *path2, int progress);

int get_param_sfo_field(char *file, char *field, char *value);

void draw_stars();
void draw_bare_xmb(int _xmb_icon);
void draw_whole_xmb(u8 mode);
void draw_xmb_clock(u8 *buffer, const int _xmb_icon);
void reset_xmb_checked();
void draw_xmb_icon_text(int _xmb_icon);
void draw_xmb_bare(u8 _xmb_icon, u8 _all_icons, bool recursive, int _sub_level);

void draw_fileman();
void apply_theme (const char *theme_file, const char *theme_path);
void redraw_column_texts(int _xmb_icon);
void reset_xmb(u8 _flag);

void fix_perm_recursive(const char* start_path);
void change_opacity(u8 *buffer, int delta, u32 size);

int vert_indx=0, vert_texture_indx=0;
void flip(void);

//misc thread

sys_ppu_thread_t addmus_thr_id;
static void add_music_column_thread_entry( uint64_t arg );
int is_music_loading=0;

sys_ppu_thread_t addpic_thr_id;
static void add_photo_column_thread_entry( uint64_t arg );
int is_photo_loading=0;

sys_ppu_thread_t addret_thr_id;
static void add_retro_column_thread_entry( uint64_t arg );
int is_retro_loading=0;

sys_ppu_thread_t addvid_thr_id;
static void add_video_column_thread_entry( uint64_t arg );
int is_video_loading=0;

int is_decoding_jpg=0;
void load_jpg_threaded(int _xmb_icon, int cn);

int is_decoding_png=0;
void load_png_threaded(int _xmb_icon, int cn);

static void download_thread_entry( uint64_t arg );
sys_ppu_thread_t download_thr_id;

static void misc_thread_entry( uint64_t arg );
sys_ppu_thread_t misc_thr_id;

static void jpg_thread_entry( uint64_t arg );
sys_ppu_thread_t jpgdec_thr_id;

static void png_thread_entry( uint64_t arg );
sys_ppu_thread_t pngdec_thr_id;

const int32_t misc_thr_prio  = 1600;	
const size_t app_stack_size  = 32768;

//int32_t main_video( void );
bool g_exec;
int is_game_loading=0;
int is_any_xmb_column=0;
u8 drawing_xmb=0;
float angle=0.f;

static int old_fi=-1;
int counter_png=0;
int is_reloaded=0;
u32 fdevices=0;
u32 fdevices_old=0;
int take_screenshot=0;
//int sub_menu_open=0;
int pb_step=429;
int never_used_pfs=1;
int repeat_counter1=60; //wait before repeat
int repeat_counter2=5; // repeat after pause
int repeat_counter3=1; // accelerate repeat (multiplier)
float repeat_counter3_inc=0.f;
int key_repeat=0;
char time_result[2];
int seconds_clock=0;
u8 xmb_legend_drawn=0;
u8 use_analog=0;
u8 join_copy=0;
u8 xmb_settings_sel=0;

typedef struct
{
	char split_file[512];
	char cached_file[512];
} cached_files_struct;
cached_files_struct file_to_join[10];
u8 max_joined=0;

char d1[512], d2[512], df[512];
unsigned char bdemu[0x1380];
unsigned char mouse[5120];
u8 bdemu2_present=0;
//int to_reboot=0;
int search_mmiso=0;
unsigned int debug_print=102030;
long long int last_refresh=0;
//long long int last_refreshD=0;
//char mmbin[64];
//int use_png_alpha=0;
CellSysCacheParam cache_param ;

//webbrowser
int mc_size;
bool exit_flag_ = false;
volatile int www_running = 0;
char www_info[256];
static CellWebBrowserConfig2 config_full;


int dim=0, dimc=0;
int c_opacity=0xff, c_opacity_delta=-1;
int c_opacity2=0xff;
int b_box_opaq= 0xf8; //for display mode 2
int b_box_step= -4;
float c_firmware=3.41f;
int use_symlinks=0;
int ftp_service=0;
int ftp_clients=0;
bool http_active=false;
//nethost
char get_cmd[1024];
static char buf[BUF_SIZE];
#if (CELL_SDK_VERSION>0x210001)
static u8 *buf2;
#endif
u32 BUF_SIZE2=0;

struct stat s3;

time_t time_start;
uint32_t blockSize;
uint64_t freeSize;
uint64_t freeSpace;

//FONTS
typedef struct SampleRenderTarget {
	CellFontRenderer      Renderer;
	CellFontRenderSurface Surface; 
}SampleRenderWork;

static const CellFontLibrary* freeType;
static Fonts_t* fonts;                 
//static FontBitmaps_t FontBitmaps;      
static SampleRenderWork RenderWork;    
int legend_y=760, legend_h=96, last_selected, rnd, game_last_page;

int dox_width=256;
int dox_height=256;

int dox_cross_x=15;
int dox_cross_y=14;
int dox_cross_w=34;
int dox_cross_h=34;

int dox_circle_x=207;
int dox_circle_y=14;
int dox_circle_w=34;
int dox_circle_h=34;


int dox_triangle_x=80;
int dox_triangle_y=14;
int dox_triangle_w=34;
int dox_triangle_h=34;

int dox_square_x=143;
int dox_square_y=14;
int dox_square_w=36;
int dox_square_h=34;

int dox_start_x=12;
int dox_start_y=106;
int dox_start_w=42;
int dox_start_h=36;

int dox_select_x=72;
int dox_select_y=108;
int dox_select_w=50;
int dox_select_h=34;

int dox_ls_x=132;
int dox_ls_y=70;
int dox_ls_w=56;
int dox_ls_h=56;

int dox_rs_x=196;
int dox_rs_y=196;
int dox_rs_w=58;
int dox_rs_h=56;

//int dox_pad_x=0;
//int dox_pad_y=200;
//int dox_pad_w=64;
//int dox_pad_h=50;

int dox_pad_x=7;
int dox_pad_y=200;
int dox_pad_w=53;
int dox_pad_h=53;

int dox_l1_x=130;
int dox_l1_y=143;
int dox_l1_w=60;
int dox_l1_h=24;

int dox_r1_x=194;
int dox_r1_y=143;
int dox_r1_w=60;
int dox_r1_h=24;

int dox_l2_x=2;
int dox_l2_y=143;
int dox_l2_w=62;
int dox_l2_h=24;

int dox_r2_x=66;
int dox_r2_y=143;
int dox_r2_w=62;
int dox_r2_h=24;

int dox_l3_x=68;
int dox_l3_y=196;
int dox_l3_w=58;
int dox_l3_h=56;

int dox_r3_x=132;
int dox_r3_y=196;
int dox_r3_w=58;
int dox_r3_h=56;

//white circle
int dox_rb1u_x=192;
int dox_rb1u_y=70;
int dox_rb1u_w=32;
int dox_rb1u_h=31;

//white circle selected
int dox_rb1s_x=192;
int dox_rb1s_y=101;
int dox_rb1s_w=32;
int dox_rb1s_h=31;

//gray circle
int dox_rb2u_x=224;
int dox_rb2u_y=70;
int dox_rb2u_w=31;
int dox_rb2u_h=31;

//gray circle selected
int dox_rb2s_x=224;
int dox_rb2s_y=101;
int dox_rb2s_w=31;
int dox_rb2s_h=31;

//selection circle
int dox_rb3s_x=177;
int dox_rb3s_y=40;
int dox_rb3s_w=31;
int dox_rb3s_h=30;

//attention sign
int dox_att_x=1;
int dox_att_y=65;
int dox_att_w=44;
int dox_att_h=39;

//white arrow
int dox_arrow_w_x=44;
int dox_arrow_w_y=41;
int dox_arrow_w_w=44;
int dox_arrow_w_h=44;

//black arrow
int dox_arrow_b_x=87;
int dox_arrow_b_y=58;
int dox_arrow_b_w=44;
int dox_arrow_b_h=44;

static int unload_modules();
void draw_text_stroke(float x, float y, float size, u32 color, const char *str);
int main_mp3_th( char *my_mp3);
void main_mp3( char *temp_mp3);

int my_game_delete(char *path);
int my_game_copy(char *path, char *path2);
#if (CELL_SDK_VERSION>0x210001)
int my_game_copy_pfsm(char *path, char *path2);
#endif

#define	GAME_INI_VER	"MMGI0100" //PS3GAME.INI	game flags (submenu)
#define	GAME_STATE_VER	"MMLS0102" //LSTAT.BIN		multiMAN last state data
#define	GAME_LIST_VER	"MMGL0103" //LLIST.BIN		cache for game list
#define	XMB_COL_VER		"MMXC0104" //XMBS.00x		xmb[?] structure (1 XMMB column) 13 599 208

char current_version[9]="02.00.04";
char current_version_NULL[10];
char versionUP[64];


char hdd_folder[64]="/dev_hdd0/GAMES/";

char last_play[512];
char last_play_dir[512];
char last_play_sfo[512];

//char multi_loading[38]="multiMAN is loading, please wait...";
int first_launch=1;
char app_path[32];
char app_temp[128];
char app_usrdir[64];
char app_homedir[64];
char options_ini[128];
char options_bin[128];
char color_ini[128];
char url_base[28];
char list_file[128];
char list_file_state[128];
char snes_self[512];
char snes_roms[512];
char genp_self[512];
char genp_roms[512];
char fceu_self[512];
char fceu_roms[512];
char vba_self[512];
char vba_roms[512];

char ini_hdd_dir[64]="/dev_hdd0/GAMES/";
char ini_usb_dir[64]="GAMES";
char hdd_home[128]="/dev_hdd0/GAMES";
//aux search folders
char hdd_home_2[128]="/_skip_";
char hdd_home_3[128]="/_skip_";
char hdd_home_4[128]="/_skip_";
char hdd_home_5[128]="/_skip_";

char usb_home[128]="/GAMES";
char usb_home_2[128]="/GAMEZ";
char usb_home_3[128]=" ";
char usb_home_4[128]=" ";
char usb_home_5[128]=" ";

static char cache_dir[128]=" ";
char covers_retro[128]=" ";
char covers_dir[128]=" ";
char themes_dir[128]=" ";
char themes_web_dir[128]=" ";
char game_cache_dir[128];
char update_dir[128]="/_skip";
char download_dir[128]="/_skip";
int verify_data=1;
int usb_mirror=0;
int scan_for_apps=0;
int date_format=0; // 1=MM/DD/YYYY, 2=YYYY/MM/DD
int time_format=1; // 0=12h 1=24h
int progress_bar=1;
int dim_setting=5; //5 seconds to dim titles
int ss_timeout=2;
int ss_timer=0;
int egg=0;
int ss_timer_last=0;
int direct_launch_forced=0;
int clear_activity_logs=1;
int load_custom_payload=0;
int	sc36_path_patch=0;
//int load_last_state=1;
int lock_display_mode=-1;
int lock_fileman=0;
int scale_icon_h=0;
char fm_func[32];
int parental_level=0;
int parental_pin_entered=0;
char parental_pass[16];
int pfs_enabled=0;
int xmb_sparks=1;
int xmb_game_bg=1;
int xmb_cover=1;
int xmb_popup=1;
u8 gray_poster=1;
u8 confirm_with_x=1;
u8 hide_bd=0;

#define	BUTTON_SELECT		(1<<0)
#define	BUTTON_L3			(1<<1)
#define	BUTTON_R3			(1<<2)
#define	BUTTON_START		(1<<3)
#define	BUTTON_UP			(1<<4)
#define	BUTTON_RIGHT		(1<<5)
#define	BUTTON_DOWN			(1<<6)
#define	BUTTON_LEFT			(1<<7)
#define	BUTTON_L2			(1<<8)
#define	BUTTON_R2			(1<<9)
#define	BUTTON_L1			(1<<10)
#define	BUTTON_R1			(1<<11)
#define	BUTTON_TRIANGLE		(1<<12)
#define	_BUTTON_CIRCLE		(1<<13)
#define	_BUTTON_CROSS		(1<<14)
#define	BUTTON_SQUARE		(1<<15)

u16 BUTTON_CROSS =	_BUTTON_CROSS;	 //(1<<12)
u16 BUTTON_CIRCLE=	_BUTTON_CIRCLE;	 //(1<<13)

u8 init_finished=0;

char userBG[64];
char auraBG[64];
char avchdBG[64];
char blankBG[64];
char playBG[64];
char legend[64];
char xmbicons[64];
char xmbicons2[64];
char xmbdevs[64];
char xmbbg[64];
//char playBGL[64];
char playBGR[64];
char avchdIN[64];
char avchdMV[64];
char helpNAV[64];
char helpMME[128];

int abort_rec=0;

char disclaimer[64], bootmusic[64];
char ps2png[64], dvdpng[64];
//char sys_cache[512];

char mouseInfo[128];//char mouseInfo2[128];

	u8 *text_bmp=NULL;
	u8 *text_bmpS=NULL;
//	u8 *text_bmpBG=NULL;
	u8 *text_bmpUBG=NULL;

	u8 *text_USB=NULL;
	u8 *text_HDD=NULL;
	u8 *text_BLU_1=NULL;
	u8 *text_NET_6=NULL; 
	u8 *text_OFF_2=NULL; 
	u8 *text_FMS=NULL; 

	u8 *text_DOX=NULL; 
	u8 *text_MSG=NULL; 

	u8 *text_CFC_3=NULL; 
	u8 *text_SDC_4=NULL; 
	u8 *text_MSC_5=NULL; 
	u8 *text_bmpUPSR=NULL;
	u8 *text_bmpIC;
	u8 *text_TEMP;
	u8 *text_FONT;

	u8 *xmb_col;
	u8 *xmb_clock;
	u8 *xmb_icon_globe	=	NULL;
	u8 *xmb_icon_help	=	NULL;
	u8 *xmb_icon_quit	=	NULL;
	u8 *xmb_icon_star	=	NULL;
	u8 *xmb_icon_star_small = NULL;
	u8 *xmb_icon_retro	=	NULL;
	u8 *xmb_icon_ftp	=	NULL;
	u8 *xmb_icon_folder	=	NULL;
	u8 *xmb_icon_desk	=	NULL;
	u8 *xmb_icon_hdd	=	NULL;
	u8 *xmb_icon_blu	=	NULL;
	u8 *xmb_icon_tool	=	NULL;
	u8 *xmb_icon_note	=	NULL;
	u8 *xmb_icon_film	=	NULL;
	u8 *xmb_icon_photo	=	NULL;
	u8 *xmb_icon_update	=	NULL;
	u8 *xmb_icon_restart=	NULL;
	u8 *xmb_icon_dev	=	NULL;
	u8 *xmb_icon_ss		=	NULL;
	u8 *xmb_icon_showtime	=	NULL;
	u8 *xmb_icon_theme		=	NULL;


FILE *fpV;
int do_move=0;
int no_real_progress=0;
int is_sliding=0;
float new_offset=0.025f; //for filemanager overscan
int payload=0;
char payloadT[2];
int socket_handle;
int no_callback=1;

int      portNum = -1;
int	multiStreamStarted=0;

//OSK
CellOskDialogCallbackReturnParam OutputInfo;
CellOskDialogInputFieldInfo inputFieldInfo;
uint16_t Result_Text_Buffer[128 + 1];
//sys_memory_container_t container=NULL;
int enteredCounter = 0;
char new_file_name[128];

char iconHDD[64], iconUSB[64], iconBLU[64], iconNET[64], iconOFF[64], iconSDC[64], iconCFC[64], iconMSC[64];
char this_pane[256], other_pane[256];

 //char filename[1024];

 int cover_available=0, cover_available_1=0, cover_available_2=0, cover_available_3=0, cover_available_4=0, cover_available_5=0, cover_available_6=0;
 int scan_avchd=1;
 int expand_avchd=1;
 int mount_bdvd=1;
 int mount_hdd1=1;
// int mount_dev_flash=0;
 int animation=3;
 int direct_launch=1;
 int disable_options=0, force_disable_copy=0;
 int download_covers=0;
 float overscan=0.0f;
// char overscan_head[16];
// char overscanS[128]="0";
// int overscanX=0, overscanY=0;
//const char *mount_point="/dev_bdvd";
 int force_update_check=0;
 int display_mode=0; // 0-All titles, 1-Games only, 2-AVCHD/Video only
 int game_bg_overlay=1;


	typedef struct
	{
		path_open_entry entries[24];
		char arena[0x9000];
	} path_open_table;

	typedef struct
	{
		path_open_entry entries[2];
		char arena[0x600];
	} path_open_table2;

	path_open_table open_table;
	uint64_t dest_table_addr;

char gameID[512];

#define MAX_LIST 640
t_menu_list menu_list[MAX_LIST];

void DBPrintf( const char *string)
{
	if(debug_print==102030) return;
#if (CELL_SDK_VERSION>0x210001)
	cellConsolePrintf(debug_print, string);	
#else
	(void) string;
#endif
}

static void _Multi_Stream_Update_Thread(uint64_t param);

#define MAX_LIST_OPTIONS 128
typedef struct
{
	u8		enabled;
	char 	label[64];
	char 	value[512];
}
t_opt_list;
t_opt_list opt_list[MAX_LIST_OPTIONS];
u8 opt_list_max=0;

int open_select_menu(char *_caption, int _width, t_opt_list *list, int _max, u8 *buffer, int _max_entries, int _centered);
int open_list_menu(char *_caption, int _width, t_opt_list *list, int _max, int _x, int _y, int _max_entries, int _centered);
int open_dd_menu(char *_caption, int _width, t_opt_list *list, int _max, int _x, int _y, int _max_entries);
int context_menu(char *_cap, int _type, char *c_pane, char *o_pane);

#define MAX_PANE_SIZE 2560
typedef struct
{
	//unsigned flags;
	u8 	type; //0-dir 1-file
	char 	name[128];
	char 	path[512];
	char	entry[128]; // __0+name for dirs and __1+name for files - used for sorting dirs first
	int64_t	size;
	time_t 	time;
	char	datetime[10];
	mode_t  mode;
	u8		selected;
}
t_dir_pane;
t_dir_pane pane_l[MAX_PANE_SIZE]; //left directory pane
t_dir_pane pane_r[MAX_PANE_SIZE]; //right directory pane
int max_dir_l=0;
int max_dir_r=0;

#define MAX_PANE_SIZE_BARE 2560
typedef struct
{
	char 	name[128];
	char 	path[512];
	int64_t	size;
	time_t 	time;
}
t_dir_pane_bare;


int state_read=1;
int state_draw=1;
int draw_legend=1;

char current_left_pane[512]="/", current_right_pane[512]="/", my_txt_file[512];
int first_left=0, first_right=0, viewer_open=0;
int max_menu_list=0;

typedef struct
{
	char host[64];
//	char root[512];
	char name[512];
	char friendly[32];
	int	 port;
}
net_hosts;
net_hosts host_list[10]; 
int max_hosts=0;


#define MAX_F_FILES 4000
typedef struct
{
	char 		path[512];
	uint64_t	size;
}
f_files_stru;

f_files_stru f_files[MAX_F_FILES];
int max_f_files=0;

int file_counter=0; // to count files
int abort_copy=0; // abort process
int num_directories=0, num_files_big=0, num_files_split=0;

typedef struct
{
	char 	label[256];
    float	x;
	float	y;
	float	scale;
	float	weight;
	float	slant;
	int		font;
	float	hscale;
	float	vscale;
	int		centered;
	float	cut;
	uint32_t color;
}
ttf_labels;

ttf_labels ttf_label[512];
int max_ttf_label=0;

int mode_list=0;
u32 forcedevices=0xffff;

u8 fm_sel=0;
u8 fm_sel_old=15;

int game_sel=0;
int game_sel_last=0;
int cover_mode=8, initial_cover_mode=8, user_font=1, last_cover_mode=8;
int dir_mode=2;
int game_details=2;
int bd_emulator=1;
int net_available=0;
union CellNetCtlInfo net_info;
int net_avail=1;
int theme_sound=1;

int copy_file_counter=0;
//int64_t 
uint64_t copy_global_bytes=0x00ULL;
uint64_t global_device_bytes=0x00ULL;
int lastINC=0, lastINC3=0, lastINC2=0;

uint64_t memvaloriginal = 0x386000014E800020ULL;
uint64_t memvaltemp	= 0x386000014E800020ULL;
uint64_t memvalnew	= 0xE92296887C0802A6ULL;
int patchmode = -1;

using namespace cell::Gcm;

#define	IS_DISC			(1<<0)
#define	IS_HDD			(1<<1)
#define	IS_USB			(1<<2)

#define	IS_DBOOT		(1<<5)
#define	IS_BDMIRROR		(1<<6)
#define	IS_PATCHED		(1<<7)
#define	IS_FAV			(1<<8)
#define	IS_EXTGD		(1<<9)

#define	IS_PS3			(1<<13)
#define	IS_LOCKED		(1<<14)
#define	IS_PROTECTED	(1<<15)

#define IS_OTHER		 (0<<16)
#define IS_ACTION		 (1<<16)
#define IS_ADVENTURE	 (2<<16)
#define IS_FAMILY		 (3<<16)
#define IS_FIGHTING		 (4<<16)
#define IS_PARTY		 (5<<16)
#define IS_PLATFORM		 (6<<16)
#define IS_PUZZLE		 (7<<16)
#define IS_ROLEPLAY		 (8<<16)
#define IS_RACING		 (9<<16)
#define IS_SHOOTER		(10<<16)
#define IS_SIM			(11<<16)
#define IS_SPORTS		(12<<16)
#define IS_STRATEGY		(13<<16)
#define IS_TRIVIA		(14<<16)
#define IS_HOMEBREW		(15<<16)

#define IS_GENRE_MASK	 (15<<16)
//#define IS_GENRE_CLEAR  ~(IS_GENRE_MASK) // user&= ~IS_GENRE_MASK

/*Action
Adventure
Family
Fighting
Party
Platform
Puzzle
Role Playing
Racing
Shooter
Sim
Sports
Strategy
Trivia
Homebrew
Other*/

static const char genre[][16] = { 
"Other",
"Action",
"Adventure",
"Family",
"Fighting",
"Party",
"Platform",
"Puzzle",
"Role Playing",
"Racing",
"Shooter",
"Sim",
"Sports",
"Strategy",
"Trivia",
"3D Support"
};

#define CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF	(0<<7)
#define CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON	(1<<7)
#define CELL_MSGDIALOG_TYPE_PROGRESSBAR_DOUBLE	(2<<12)

uint8_t padLYstick=0, padLXstick=0, padRYstick=0, padRXstick=0, BDremote=0;

double mouseX=0.5f, mouseY=0.5f, mouseYD=0.0000f, mouseXD=0.0000f, mouseYDR=0.0000f, mouseXDR=0.0000f, mouseYDL=0.0000f, mouseXDL=0.0000f;
uint8_t xDZ=30, yDZ=30;
uint8_t xDZa=30, yDZa=30;

unsigned cmd_pad= 0;

//int ;
float offY=0.0f, BoffY=0.0f, offX=0.0f, incZ=0.7f, BoffX=0.0f, slideX=0.0f;
int slideX1=0, slideX2=0, slideX3=0, slide_step=0, stepX=0, stepX1=0, stepX2=0, stepY=0, stepY1=0, stepY2=0, istepX=0, istepX1=0, istepX2=0, istepY=0, istepY1=0, istepY2=0;
static void *host_addr;
//static void *fast_files_mem;

static uint32_t syscall35(const char *srcpath, const char *dstpath);
static void syscall_mount(const char *path,  int mountbdvd);

int load_texture(u8 *data, char *name, uint16_t dw);

 
 time_t rawtime;
 struct tm * timeinfo;

int xmb_bg_show=0;
int xmb_bg_counter=0;

#define MAX_STARS 128
typedef struct
{
    u16		x;
	u16		y;
	u8		bri;
	u8		size;
}
stars_def;
stars_def stars[MAX_STARS];

#define XMB_TEXT_WIDTH 912
#define XMB_TEXT_HEIGHT 74
#define MAX_XMB_TEXTS (1920 * 1080 * 2) / (XMB_TEXT_WIDTH * XMB_TEXT_HEIGHT)
typedef struct
{
	bool used;
	u8	*data; //pointer to image
}
xmbtexts;
xmbtexts xmb_txt_buf[MAX_XMB_TEXTS];
int xmb_txt_buf_max=0;

#define XMB_THUMB_WIDTH 408
#define XMB_THUMB_HEIGHT 408
#define MAX_XMB_THUMBS (1920 * 1080 * 4) / (XMB_THUMB_WIDTH * XMB_THUMB_HEIGHT)
typedef struct
{
	int used;
	int column;
	u8	*data; //pointer to image
}
xmbthumbs;
xmbthumbs xmb_icon_buf[MAX_XMB_THUMBS];
int xmb_icon_buf_max=0;

#define MAX_XMB_OPTIONS 12
typedef struct
{
//	u8 type; //0-list 1-text
	char label[32];
	char value[8];

}
xmbopt;

sys_addr_t vm;

#define MAX_XMB_MEMBERS 2048
typedef struct
{
	u8 type; //0 unkn, 1 ps3 game, 2 video from gamelist, 3 showtime vid, 4 music, 5 photo, 6 function, 7 setting, 8 snes rom, 9 fceu rom, 10 vba rom, 11 genp rom, 12 fbanext
	u8 status; //0 idle, 1 loading, 2 loaded
	bool is_checked;
	int game_id; //pointer to menu_list[id]
	int game_split;
	int game_user_flags;
	char name[128];
	char subname[96];
	u8 option_size;
	u8 option_selected;
	char optionini[32];
	xmbopt	option[MAX_XMB_OPTIONS];
	int	data; //pointer to text image (xmbtexts) xmb_txt_buf
	int icon_buf;
	u8	*icon; //pointer to icon image
	u16	iconw;
	u16	iconh;
	char file_path[128]; //path to entry file
	char icon_path[128]; //path to entry icon
}
xmbmem;

#define MAX_XMB_ICONS 10
typedef struct
{
	u8		init;
    u16		size;
	u16		first;
	u8		*data;
	char	name[32];
	u8		group; //0-no grouping

	xmbmem	member[MAX_XMB_MEMBERS];
}
xmb_def;
xmb_def xmb[MAX_XMB_ICONS]; //xmb[0] can be used as temp column when doing grouping

int xmb_icon=6;				//1 home, 2 settings, 3 photo, 4 music, 5 video, 6 game, 7 faves, 8 retro, 9 web
int xmb_icon_last=6;
u16 xmb_icon_last_first=0;
int xmb_slide=0;
int xmb_slide_y=0;
int xmb_slide_step=0;
int xmb_slide_step_y=0;
int xmb_sublevel=0;

#define MAX_GROUPS (4)
u8 is_group_mode=0;


#define MAX_WWW_THEMES 128
typedef struct
{
	u8 type;
	char name[64];
	char pkg[128];
	char img[128];
	char author[32];
	char mmver[16];
	char info[64];
}
theme_def;
theme_def www_theme[MAX_WWW_THEMES];
u8 max_theme=0;

int open_theme_menu(char *_caption, int _width, theme_def *list, int _max, int _x, int _y, int _max_entries, int _centered);

void draw_xmb_icons(xmb_def *_xmb, const int _xmb_icon, int _xmb_x_offset, int _xmb_y_offset, const bool _recursive, int sub_level);

#define MAX_DOWN_LIST (128) //queue for background downloads
typedef struct
{
	int status;
	char url[512];
	char local[512];
}
downqueue;
downqueue downloads[MAX_DOWN_LIST];
int downloads_max=0;

#define MAX_MSG (128) //queue for pop-up messages
typedef struct
{
	int status;
	char line1[64];
	char line2[64];
	char line3[64];
}
msgqueue;
msgqueue message[MAX_MSG];
int max_message=0;

#define MAX_MP3 1024//MAX_XMB_MEMBERS
typedef struct
{
	char path[512];
//	u64	size;
//	u64 pos;
}
mp3_playlist_type;
mp3_playlist_type mp3_playlist[MAX_MP3]; 
int max_mp3=0;
int current_mp3=0;

char *tmhour(int _hour)
{
	int th=_hour;
	if(time_format) th=_hour;//sprintf(time_result, "%2d", _hour);
	else 
	{
		if(_hour>11) th=_hour-12;
		if(!th) th=12;
	}
	sprintf(time_result, "%2d", th);
	return time_result;
}

void set_xo()
{
	if(confirm_with_x==1)
	{
		BUTTON_CROSS =	_BUTTON_CROSS;	 //(1<<12)
		BUTTON_CIRCLE=	_BUTTON_CIRCLE;	 //(1<<13)
		dox_cross_x=15;
		dox_cross_y=14;
		dox_cross_w=34;
		dox_cross_h=34;

		dox_circle_x=207;
		dox_circle_y=14;
		dox_circle_w=34;
		dox_circle_h=34;
	}
	else
	{
		BUTTON_CROSS =	_BUTTON_CIRCLE;
		BUTTON_CIRCLE=	_BUTTON_CROSS;

		dox_circle_x=15;
		dox_circle_y=14;
		dox_circle_w=34;
		dox_circle_h=34;

		dox_cross_x=207;
		dox_cross_y=14;
		dox_cross_w=34;
		dox_cross_h=34;

	}
}

int rndv(int maxval)
{
	return (int) ((float)rand() * ((float)maxval / (float)RAND_MAX));
}

#if (CELL_SDK_VERSION>0x210001)
static void usbdev_init(int check)
{
	FILE *fid;
	int vid, pid;
	DEV_INFO *usbdev;
	int32_t r;
	char usbcfg[128];
	sprintf(usbcfg, "%s/USB.CFG", app_usrdir);

	fid = fopen(usbcfg, "r");
	if (!fid)
		return;

	while (fscanf(fid, "%x:%x:%i", &vid, &pid, &max_usb_volumes) != EOF) {
		if ((usbdev = (DEV_INFO *)malloc(sizeof(DEV_INFO))) == NULL)
			break;

		if(check)
		{
			if ((r = PfsmDevAdd(vid, pid, &usbdev->dev))) {
				free(usbdev);
			} else {
				usbdev->next = dev_info;
				dev_info = usbdev;
			}
		}
	}

	fclose(fid);

}

static void usbdev_uninit(void)
{
	DEV_INFO *usbdev;

	while ((usbdev = dev_info)) {
		dev_info = usbdev->next;
		PfsmDevDel(&usbdev->dev);
		free(usbdev);
	}
}
#endif

void flipc(int _fc)
{
	int flipF;
	for(flipF = 0; flipF<_fc; flipF++) 		
	{
		sys_timer_usleep(3336);
		flip();
		//cellSysutilCheckCallback();
	}
}

void get_free_memory()
{
	system_call_1(352, (uint64_t) &meminfo);
}

u32 new_pad=0, old_pad=0;
uint8_t old_status=0;
uint8_t old_status_k=0;
int pad_num=0;

static int pad_read( void )
{
	ss_timer=(time(NULL)-ss_timer_last);

	int ret;
	u32 padd;

	static u32 old_info = 0;
	cmd_pad= 0;


static CellPadData databuf;

static CellMouseInfo Info;
static CellMouseData Data;

static CellKbInfo info;
static CellKbData kdata;

float mouse_speed=0.001f;

uint32_t old_info_m = 0;
uint32_t old_info_k = 0;

//check for keyboard input


		if(cellKbSetReadMode (0, CELL_KB_RMODE_INPUTCHAR) != CELL_KB_OK) goto read_mouse;
		if(cellKbSetCodeType (0, CELL_KB_CODETYPE_ASCII)  != CELL_KB_OK) goto read_mouse;
		if(cellKbGetInfo (&info) != CELL_KB_OK) goto read_mouse;

		if((info.info & CELL_KB_INFO_INTERCEPTED) && 
		   (!(old_info_k & CELL_KB_INFO_INTERCEPTED))){
			old_info_k = info.info;
		}else if((!(info.info & CELL_KB_INFO_INTERCEPTED)) &&
				 (old_info_k & CELL_KB_INFO_INTERCEPTED)){
			old_info_k = info.info;
		}
        if (info.status[0] == CELL_KB_STATUS_DISCONNECTED) goto read_mouse;
		if (cellKbRead (0, &kdata)!=CELL_KB_OK) goto read_mouse;
        if (kdata.len == 0) goto read_mouse;

//		sprintf(mouseInfo, "Keyboard: Buttons : %02X %c", kdata.keycode[0], kdata.keycode[0]); 
	
        old_status_k = info.status[0];


		padd = 0;
		if(kdata.keycode[0]>0x2f && kdata.keycode[0]<0x37) pad_num=kdata.keycode[0]-0x30; //0-6 switch pad port
		if(kdata.keycode[0]>0x8039 && kdata.keycode[0]<0x8040 && cover_mode!=5) //F1-F6 switch cover mode
		{
			cover_mode=kdata.keycode[0]-0x803a; old_fi=0; 
			if(cover_mode!=4) {
					load_texture(text_BLU_1, iconBLU, 320);
					load_texture(text_NET_6, iconNET, 320);
					load_texture(text_OFF_2, iconOFF, 320);
					load_texture(text_CFC_3, iconCFC, 320);
					load_texture(text_SDC_4, iconSDC, 320);
					load_texture(text_MSC_5, iconMSC, 320);
			}
			old_fi=-1;
			counter_png=0;
			goto pad_out;
		}

		if(cover_mode==5)
		{
			if(kdata.keycode[0]==0x8049 || kdata.keycode[0]==0xC049) padd = (1<<11); //INS= R1
			else if(kdata.keycode[0]==0x803b) padd = padd | (1<<14)|(1<<0); //F2= select+x
			else if(kdata.keycode[0]==0x0009) padd = padd | (1<<10); //TAB= L1

			else if(kdata.keycode[0]==0xC050) mouseX-=0.02f; //LEFT (NUM BLOCK)
			else if(kdata.keycode[0]==0xC04F) mouseX+=0.02f; //RIGHT
			else if(kdata.keycode[0]==0xC052) mouseY-=0.02f; //UP
			else if(kdata.keycode[0]==0xC051) mouseY+=0.02f; //DOWN

			else if(kdata.keycode[0]==0x78 || kdata.keycode[0]==0x58) padd = padd | (1<<13) | (1<<0); //x = select+circle / move
			else if(kdata.keycode[0]==0x76 || kdata.keycode[0]==0x56) padd = padd | (1<<2); //v = R3 / HEX view
			else if(kdata.keycode[0]==0xC04B) padd = padd | (1<<8); //PGUP->L2
			else if(kdata.keycode[0]==0xC04E) padd = padd | (1<<9); //PGDN->R2

		}

		if(kdata.keycode[0]==0x8050) padd = padd | (1<<7); //LEFT
		else if(kdata.keycode[0]==0x804F) padd = padd | (1<<5); //RIGHT
		else if(kdata.keycode[0]==0x8052) padd = padd | (1<<4); //UP
		else if(kdata.keycode[0]==0x8051) padd = padd | (1<<6); //DOWN

		else if(kdata.keycode[0]==0x804B) padd = padd | (1<<7); //PGUP->LEFT
		else if(kdata.keycode[0]==0x804E) padd = padd | (1<<5); //PGDN->RIGHT

		else if(kdata.keycode[0]==0x804C || kdata.keycode[0]==0xC04C) padd = padd | (1<<15); //DEL
		else if(kdata.keycode[0]==0x400A || kdata.keycode[0]==0x000A) padd = padd | (1<<14); //ENTER
		else if(kdata.keycode[0]==0x8029) padd = padd | (1<<12); //ESC->TRIANGLE

		else if(kdata.keycode[0]==0x402b) padd = padd | (1<<3) | (1<<4); //NUM+ = START-UP
		else if(kdata.keycode[0]==0x402d) padd = padd | (1<<3) | (1<<6); //NUM- = START-DOWN
		else if(kdata.keycode[0]==0x402f) padd = padd | (1<<3) | (1<<7); // / = START-left
		else if(kdata.keycode[0]==0x402a) padd = padd | (1<<3) | (1<<5); // *- = START-right


		else if(kdata.keycode[0]==0x8044) padd = padd | (1<<0) | (1<<10); //F11 = SELECT+L1
		else if(kdata.keycode[0]==0x8045 || kdata.keycode[0]==0x0009) padd = padd | (1<<10); //F12 = L1
		else if(kdata.keycode[0]==0x8043) padd = padd | (1<<2); //F10 = R3
		else if(kdata.keycode[0]==0x8042) padd = padd | (1<<9); //F9 = R2
		else if(kdata.keycode[0]==0x8041) padd = padd | (1<<11); //F8 = R1
		else if(kdata.keycode[0]==0x8040 || kdata.keycode[0]==0x8039) padd = padd | (1<<1); //F7 = L3
		else if(kdata.keycode[0]==0x43 || kdata.keycode[0]==0x63) padd = padd | (1<<13); //c = circle / copy	

		goto pad_out;

// check for mouse input

read_mouse:
		ret = cellMouseGetInfo (&Info);

        if((Info.info & CELL_MOUSE_INFO_INTERCEPTED) &&
           (!(old_info_m & CELL_MOUSE_INFO_INTERCEPTED))){
            old_info_m = Info.info;
        }else if((!(Info.info & CELL_MOUSE_INFO_INTERCEPTED)) &&
                 (old_info_m & CELL_MOUSE_INFO_INTERCEPTED)){
            old_info_m = Info.info;
        }

		if (Info.status[0] == CELL_MOUSE_STATUS_DISCONNECTED) goto read_pad;

        ret = cellMouseGetData (0, &Data);
        if (CELL_OK != ret) goto read_pad;
        if (Data.update != CELL_MOUSE_DATA_UPDATE) goto read_pad;

//		sprintf(mouseInfo, "Buttons : %02X | x-axis : %d | y-axis : %d | Wheel : %d | Tilt : %d ", Data.buttons, Data.x_axis, Data.y_axis, Data.wheel, Data.tilt); 
		old_status = Info.status[0];

		padd= ((Data.buttons & 1)<<14) | ((Data.buttons & 2)<<12); //LEFT=X / RIGHT=CIRCLE
		padd = padd | ( (Data.buttons & 4)<<8); //WHEEL=L1
		if(Data.x_axis<-1 || Data.x_axis> 1) mouseX+= Data.x_axis*mouse_speed;
		if(Data.y_axis<-1 || Data.y_axis> 1) mouseY+= Data.y_axis*mouse_speed;

		if(cover_mode!=4) {
			if(Data.wheel>0)	padd = padd | (1<<4);
			if(Data.wheel<0)	padd = padd | (1<<6);
		}
		else
		{
			if(Data.wheel>0)	padd = padd | (1<<7);
			if(Data.wheel<0)	padd = padd | (1<<5);
		}

		if(cover_mode!=5) {
			if(Data.x_axis>10)	padd = padd | (1<<5);
			if(Data.x_axis<-10)	padd = padd | (1<<7);

			if(Data.y_axis>10)	padd = padd | (1<<6);
			if(Data.y_axis<-10)	padd = padd | (1<<4);

		}

		mouseYD=0.0f; mouseXD=0.0f; 
		mouseYDL=0.0f; mouseXDL=0.0f; 
		mouseYDR=0.0f; mouseXDR=0.0f; 
		mouseXD=0; mouseYD=0;

		cellMouseClearBuf(0);

	goto pad_out;

read_pad:

#if (CELL_SDK_VERSION<=0x210001)
CellPadInfo infobuf;
ret = cellPadGetInfo(&infobuf);

	if ( ret != 0 ) 
		{
		old_pad=new_pad = 0;
		return 1;
		}

for(pad_num=0;pad_num<7;pad_num++)
	if ( infobuf.status[pad_num] == CELL_PAD_STATUS_CONNECTED ) {goto pad_ok;}

//	if ( infobuf.status[pad_num] == CELL_PAD_STATUS_DISCONNECTED ) 
		{
		old_pad=new_pad = 0;
		return 1;
		}
pad_ok:

	if((infobuf.info & CELL_PAD_INFO_INTERCEPTED) && (!(old_info & CELL_PAD_INFO_INTERCEPTED)))
		{
		old_info = infobuf.info;
		}
	else 
		if((!(infobuf.info & CELL_PAD_INFO_INTERCEPTED)) && (old_info & CELL_PAD_INFO_INTERCEPTED))
			{
			old_info = infobuf.info;
			old_pad=new_pad = 0;
			return 1;
			}

#else

CellPadInfo2 infobuf;
ret = cellPadGetInfo2(&infobuf);

/*	sprintf(mouseInfo,"(%i/%i) ", infobuf.max_connect, infobuf.now_connect );
	ret = cellPadGetData( pad_num, &databuf );
for(dn=0;dn<14;dn++){

	sprintf(mouseInfo, "%s %i=%04X ", mouseInfo, dn, databuf.button[dn]);
}

for(dn=14;dn<29;dn++){

	sprintf(mouseInfo2, "%s %i=%04X ", mouseInfo2, dn, databuf.button[dn]);
} */

	if ( ret != 0 ) 
		{
		old_pad=new_pad = 0;
		return 1;
		}

for(pad_num=0;pad_num<7;pad_num++)
	if ( infobuf.port_status[pad_num] == CELL_PAD_STATUS_CONNECTED ) {goto pad_ok;}

//	if ( infobuf.port_status[pad_num] == CELL_PAD_STATUS_DISCONNECTED ) 
		{
		old_pad=new_pad = 0;
		return 1;
		}

pad_ok:

	if((infobuf.system_info & CELL_PAD_INFO_INTERCEPTED) && (!(old_info & CELL_PAD_INFO_INTERCEPTED)))
		{
		old_info = infobuf.system_info;
		}
	else 
		if((!(infobuf.system_info & CELL_PAD_INFO_INTERCEPTED)) && (old_info & CELL_PAD_INFO_INTERCEPTED))
			{
			old_info = infobuf.system_info;
			old_pad=new_pad = 0;
			goto pad_repeat;
			}

#endif


	ret = cellPadGetData( pad_num, &databuf );

	if (ret != CELL_PAD_OK) 
		{
		old_pad=new_pad = 0;
		return 1;
		}

	if (databuf.len == 0) 
		{
		new_pad = 0;
		goto pad_repeat;
		}


	padd = ( databuf.button[2] | ( databuf.button[3] << 8 ) );

//	cellPadSetPortSetting( pad_num, CELL_PAD_SETTING_PRESS_ON | CELL_PAD_SETTING_SENSOR_ON);
//	sprintf(www_info, "%i %i %i %i %i %i %i %i %i %i ", databuf.button[2], databuf.button[3], databuf.button[24], databuf.button[25], databuf.button[14], databuf.len, databuf.button[20], databuf.button[21], databuf.button[22], databuf.button[23]);
//	sprintf(www_info, "%i %i %i %i %i %i %i %i %i ", databuf.button[0], databuf.button[1], databuf.button[2], databuf.button[3], databuf.button[4], databuf.button[5], databuf.button[6], databuf.button[7], databuf.button[8]);
//	if(databuf.button[25]==0x0b) padd|=(1 << 14); //map BD remote [enter] to [X]

	padLYstick = databuf.button[7]; 
	padLXstick = databuf.button[6]; 

	padRXstick = databuf.button[4];
	padRYstick = databuf.button[5];

	if(cover_mode!=5 && !use_analog)
	{
//		if( (padd | BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT)!=padd )
		{
			if(padLYstick<2) padd=padd | BUTTON_UP;   if(padLYstick>253) padd=padd | BUTTON_DOWN;
			if(padLXstick<2) padd=padd | BUTTON_LEFT; if(padLXstick>253) padd=padd | BUTTON_RIGHT;

			if(padRYstick<2) padd=padd | BUTTON_UP;   if(padRYstick>253) padd=padd | BUTTON_DOWN;
			if(padRXstick<2) padd=padd | BUTTON_LEFT; if(padRXstick>253) padd=padd | BUTTON_RIGHT;
		}
	}

	mouseYD=0.0f; mouseXD=0.0f; 
	mouseYDL=0.0f; mouseXDL=0.0f; 
	mouseYDR=0.0f; mouseXDR=0.0f; 
	//deadzone: x=100 y=156 (28 / 10%)

	if(padRXstick<=(128-xDZa)){
		mouseXD=(float)(((padRXstick+xDZa-128.0f))/11000.0f);//*(1.f-overscan);
		mouseXDR=mouseXD;}

	if(padRXstick>=(128+xDZa)){
		mouseXD=(float)(((padRXstick-xDZa-128.0f))/11000.0f);//*(1.f-overscan);
		mouseXDR=mouseXD;}

	if(padRYstick<=(128-yDZa)){
		mouseYD=(float)(((padRYstick+yDZa-128.0f))/11000.0f);//*(1.f-overscan); 
		mouseYDR=mouseYD;}

	if(padRYstick>=(128+yDZa)){
		mouseYD=(float)(((padRYstick-yDZa-128.0f))/11000.0f);//*(1.f-overscan); 
		mouseYDR=mouseYD;}


	if(padLXstick<=(128-xDZa)){
		mouseXD=(float)(((padLXstick+xDZa-128.0f))/11000.0f);//*(1.f-overscan);
		mouseXDL=mouseXD;}

	if(padLXstick>=(128+xDZa)){
		mouseXD=(float)(((padLXstick-xDZa-128.0f))/11000.0f);//*(1.f-overscan);
		mouseXDL=mouseXD;}

	if(padLYstick<=(128-yDZa)){
		mouseYD=(float)(((padLYstick+yDZa-128.0f))/11000.0f);//*(1.f-overscan); 
		mouseYDL=mouseYD;}

	if(padLYstick>=(128+yDZa)){
		mouseYD=(float)(((padLYstick-yDZa-128.0f))/11000.0f);//*(1.f-overscan); 
		mouseYDL=mouseYD;}

pad_out:


	c_opacity_delta=16;	dimc=0; dim=1;
	b_box_opaq= 0xfe;
	b_box_step= -4;
	new_pad=padd & (~old_pad);
	old_pad= padd;
	ss_timer=(time(NULL)-ss_timer_last);
	ss_timer_last=time(NULL);

pad_repeat:


	key_repeat=0;
	if(new_pad==0 && old_pad!=0)
	{
		repeat_counter1--;
		if(repeat_counter1<=0)
		{
			repeat_counter1=0;
			repeat_counter2--;
			if(repeat_counter2<=0)
			{
				if(repeat_counter3_inc<3.f) repeat_counter3_inc+=0.02f;
				repeat_counter3+=(int)repeat_counter3_inc;
				repeat_counter2=6;
				new_pad=old_pad;
				ss_timer=0;
				ss_timer_last=time(NULL);
			}
			key_repeat=1;
		}
	}
	else
		{repeat_counter1=60; repeat_counter2=8; repeat_counter3=1;repeat_counter3_inc=0.f;}

//	sprintf(www_info, "%i %i %i %i %i", new_pad, old_pad, repeat_counter1, repeat_counter2, key_repeat);


	return 1;
}

void screen_saver()
{
		pad_read(); c_opacity_delta=0; new_pad=0; old_pad=0; 
		int initial_skip=0;
		int www_running_old=www_running;
		while(1) {
			ss_timer=0;
			ss_timer_last=time(NULL);

			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			for(int n=0; n<MAX_STARS; n++)
			{
				draw_square(((float)stars[n].x/1920.0f-0.5f)*2.0f, (0.5f-(float)stars[n].y/1080.0f)*2.0f, (stars[n].size/1920.f), (stars[n].size/1080.f), 0.0f, (0xffffff00 | stars[n].bri));
				int move_star= rndv(10);
				if(move_star>4) 
					{stars[n].x++; } 
				else 
					{stars[n].x--; }

				if(move_star>3) 
					{stars[n].y++; } 
				if(move_star==2) 
					{stars[n].y--; }

				if(move_star>7) stars[n].bri-=4;
				if(stars[n].x>1919 || stars[n].y>1079 || stars[n].x<1 || stars[n].y<1 || stars[n].bri<4)
				{
					stars[n].x=rndv(1920);
					stars[n].y=rndv(1080);
					stars[n].bri=rndv(200);
					stars[n].size=rndv(XMB_SPARK_SIZE)+1;
				}

			}
			setRenderColor();
			flip();
			sys_timer_usleep(3336);
			//cellSysutilCheckCallback();

			initial_skip++;
			new_pad=0; old_pad=0;
			pad_read();
			if (( (new_pad || old_pad) && initial_skip>150) || www_running!=www_running_old) {// || c_opacity_delta==16
				new_pad=0; old_pad=0; break;
				}
		}
		state_draw=1; c_opacity=0xff; c_opacity2=0xff;
}

void slide_screen_left(uint8_t *buffer)
{
		int slide; char string1[64];
		for(slide=0;slide>-2048;slide-=128) {

			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			set_texture( buffer, 1920, 1080); 
			display_img((int)slide, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
			setRenderColor();
			flip();
			//cellSysutilCheckCallback();

//			new_pad=0; old_pad=0;
//			pad_read();
//			if ( (new_pad || old_pad || c_opacity_delta==16)) { new_pad=0; old_pad=0; break;}
		}
			max_ttf_label=0; memset(buffer, 0, 1920*1080*4); //0x505050
			sprintf(string1, "%s", "Please wait...");
			print_label_ex( 0.5f, 0.50f, 1.0f, 0xffffffff, string1, 1.04f, 0.0f, 15, 1.0f, 1.0f, 1);
			flush_ttf(buffer, 1920, 1080);
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			set_texture( buffer, 1920, 1080); 
			display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
			setRenderColor();
			flip();
}

void slide_screen_right(uint8_t *buffer)
{
		int slide; char string1[64];
		for(slide=0;slide<2048;slide+=128) {

			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			set_texture( buffer, 1920, 1080); 
			display_img((int)slide, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
			setRenderColor();
			flip();

//			new_pad=0; old_pad=0;
//			pad_read();
//			if ( (new_pad || old_pad || c_opacity_delta==16)) { new_pad=0; old_pad=0; break;}
		}
			max_ttf_label=0; memset(buffer, 0, 1920*1080*4);
			sprintf(string1, "%s", "Please wait...");
			print_label_ex( 0.5f, 0.50f, 1.0f, 0xffffffff, string1, 1.04f, 0.0f, 15, 1.0f, 1.0f, 1);
			flush_ttf(buffer, 1920, 1080);
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			set_texture( buffer, 1920, 1080); 
			display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
			setRenderColor();
			flip();
			//cellSysutilCheckCallback();
}


/*****************************************************/
/* DIALOG                                            */
/*****************************************************/

volatile int no_video=0;
int osk_dialog=0;
int osk_open=0;

volatile int dialog_ret=0;

u32 type_dialog_yes_no = CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BG_VISIBLE | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_YESNO
					   | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF | CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NO;

u32 type_dialog_yes_back = CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BG_VISIBLE | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK
					   | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF | CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_OK;


u32 type_dialog_ok = CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BG_VISIBLE | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK
				   | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON| CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_OK;

u32 type_dialog_no = CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BG_VISIBLE | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON;
u32 type_dialog_back = CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BG_VISIBLE | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF;

static void dialog_fun1( int button_type, void * )
{
	
	switch ( button_type ) {
	case CELL_MSGDIALOG_BUTTON_YES:
		dialog_ret=1;
		break;
	case CELL_MSGDIALOG_BUTTON_NO:
//	case CELL_MSGDIALOG_BUTTON_ESCAPE:
	case CELL_MSGDIALOG_BUTTON_NONE:
		dialog_ret=2;
		break;

	case CELL_MSGDIALOG_BUTTON_ESCAPE:
		dialog_ret=3;
		break;

	default:
		break;
	}
}
static void dialog_fun2( int button_type, void * )
{
	
	switch ( button_type ) {
		case CELL_MSGDIALOG_BUTTON_OK:
//		case CELL_MSGDIALOG_BUTTON_ESCAPE:
		case CELL_MSGDIALOG_BUTTON_NONE:
		dialog_ret=1;
		break;

	case CELL_MSGDIALOG_BUTTON_ESCAPE:
		dialog_ret=3;
		break;

	default:
		break;
	}
}

void wait_dialog()
{

	while(!dialog_ret)
		{
		//cellSysutilCheckCallback();
		if(init_finished)
		{
			if(cover_mode==8) draw_whole_xmb(xmb_icon);//draw_xmb_bare(xmb_icon, 1, 0, 0);
			else if(cover_mode==5) { cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); draw_fileman();  flip();sys_timer_usleep(1668);}
			else if(cover_mode>=0 && cover_mode<3 || cover_mode==6 || cover_mode==7)
				{
					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					set_texture( text_bmp, 1920, 1080);
					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					sys_timer_usleep(1668);
					flip();
				}
			else if(cover_mode==4)
				{
					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					set_texture( text_bmpUPSR, 1920, 1080);
					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					sys_timer_usleep(1668);
					flip();
				}
			else {flip();sys_timer_usleep(1668);}

		}
		else
			{flip();sys_timer_usleep(1668);}
		}


//	cellMsgDialogAbort();
	cellMsgDialogClose(60.0f);
	setRenderColor();
//	sys_timer_usleep(100000);
					
}

void wait_dialog_simple()
{

	while(!dialog_ret)
		{
		//cellSysutilCheckCallback();
		sys_timer_usleep(1668);
		flip();
		}

	cellMsgDialogClose(60.0f);
	setRenderColor();
					
}

int net_used_ignore()
{
	if(ftp_clients==0) return 1;
	int t_dialog_ret=dialog_ret;
	dialog_ret=0;
	cellMsgDialogAbort();
	cellMsgDialogOpen2(type_dialog_yes_no, "There are active FTP connections!\n\nAre you sure you want to continue and abort FTP transfers?", dialog_fun1, (void *) 0x0000aaaa, NULL);
	wait_dialog();
	if(dialog_ret==1) {dialog_ret=t_dialog_ret; return 1;}
	dialog_ret=t_dialog_ret;
	return 0;
}

bool is_video(char *vfile)
{
	if((strstr(vfile, ".M2TS")!=NULL || strstr(vfile, ".m2ts")!=NULL || 
		strstr(vfile, ".DIVX")!=NULL || strstr(vfile, ".divx")!=NULL || 
		strstr(vfile, ".AVI")!=NULL || strstr(vfile, ".avi")!=NULL || 

		strstr(vfile, ".MTS")!=NULL || strstr(vfile, ".mts")!=NULL || 
		strstr(vfile, ".MPG")!=NULL || strstr(vfile, ".mpg")!=NULL || 
		strstr(vfile, ".MPEG")!=NULL || strstr(vfile, ".mpeg")!=NULL || 

		strstr(vfile, ".MKV")!=NULL || strstr(vfile, ".mkv")!=NULL || 
		strstr(vfile, ".MP4")!=NULL || strstr(vfile, ".mp4")!=NULL || 
		strstr(vfile, ".VOB")!=NULL || strstr(vfile, ".vob")!=NULL || 
		strstr(vfile, ".WMV")!=NULL || strstr(vfile, ".wmv")!=NULL ||
		strstr(vfile, ".FLV")!=NULL || strstr(vfile, ".flv")!=NULL ||
		strstr(vfile, ".MOV")!=NULL || strstr(vfile, ".mov")!=NULL ||
		strstr(vfile, ".M2V")!=NULL || strstr(vfile, ".m2v")!=NULL || 
		strstr(vfile, ".BIK")!=NULL || strstr(vfile, ".bik")!=NULL ||
		strstr(vfile, ".BINK")!=NULL || strstr(vfile, ".bink")!=NULL || 
		strstr(vfile, ".264")!=NULL || strstr(vfile, ".h264")!=NULL) &&
		(strstr(vfile, ".jpg")==NULL && strstr(vfile, ".JPG")==NULL)
	  )
		return true;
	else
		return false;


}

bool is_snes9x(char *rom)
{
	if( (strstr(rom, ".smc")!=NULL || strstr(rom, ".SMC")!=NULL 
		|| strstr(rom, ".sfc")!=NULL || strstr(rom, ".SFC")!=NULL 
		|| strstr(rom, ".fig")!=NULL || strstr(rom, ".FIG")!=NULL 
		|| strstr(rom, ".gd3")!=NULL || strstr(rom, ".GD3")!=NULL 
		|| strstr(rom, ".gd7")!=NULL || strstr(rom, ".GD7")!=NULL 
		|| strstr(rom, ".dx2")!=NULL || strstr(rom, ".DX2")!=NULL 
		|| strstr(rom, ".bsx")!=NULL || strstr(rom, ".BSX")!=NULL 
		|| strstr(rom, ".swc")!=NULL || strstr(rom, ".SWC")!=NULL 
		|| strstr(rom, ".zip")!=NULL || strstr(rom, ".ZIP")!=NULL 
		|| strstr(rom, ".jma")!=NULL || strstr(rom, ".JMA")!=NULL)
		&& strstr(rom, ".jpg")==NULL && strstr(rom, ".JPG")==NULL)
		return true;
	else
		return false;
}

void launch_snes_emu(char *rom)
{
	if(!net_used_ignore()) return;
	if(stat(snes_self, &s3)<0)
	{
		dialog_ret=0;
		cellMsgDialogOpen2( type_dialog_ok, "To play SNES games you must install the latest version of SNEX9x for the PS3\xE2\x84\xA2", dialog_fun2, (void*)0x0000aaab, NULL );
		wait_dialog();
		return;
	}
	char* launchargv[2];
	memset(launchargv, 0, sizeof(launchargv));

	int len = strlen(rom);
	launchargv[0] = (char*)malloc(len + 1); strcpy(launchargv[0], rom);
	launchargv[1] = (char*)malloc(len + 1); strcpy(launchargv[1], rom);
	unload_modules();
	sys_game_process_exitspawn2((char*)snes_self, (const char**)launchargv, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_512K);
}

bool is_genp(char *rom)
{
	if( (strstr(rom, ".bin")!=NULL || strstr(rom, ".BIN")!=NULL 
		|| strstr(rom, ".smd")!=NULL || strstr(rom, ".SMD")!=NULL
		|| strstr(rom, ".md")!=NULL || strstr(rom, ".MD")!=NULL
		|| strstr(rom, ".sms")!=NULL || strstr(rom, ".SMS")!=NULL
		|| strstr(rom, ".zip")!=NULL || strstr(rom, ".ZIP")!=NULL
		|| strstr(rom, ".gen")!=NULL || strstr(rom, ".GEN")!=NULL)
		&& strstr(rom, ".jpg")==NULL && strstr(rom, ".JPG")==NULL)
		return true;
	else
		return false;
}

bool is_fceu(char *rom)
{
	if( (strstr(rom, ".nes")!=NULL || strstr(rom, ".NES")!=NULL 
		|| strstr(rom, ".zip")!=NULL || strstr(rom, ".ZIP")!=NULL
		|| strstr(rom, ".fds")!=NULL || strstr(rom, ".FDS")!=NULL
		|| strstr(rom, ".unif")!=NULL || strstr(rom, ".UNIF")!=NULL)
		&& strstr(rom, ".jpg")==NULL && strstr(rom, ".JPG")==NULL)
		return true;
	else
		return false;
}

bool is_vba(char *rom)
{
	if( (strstr(rom, ".gba")!=NULL || strstr(rom, ".GBA")!=NULL 
		|| strstr(rom, ".gbc")!=NULL || strstr(rom, ".GBC")!=NULL
		|| strstr(rom, ".gb")!=NULL || strstr(rom, ".GB")!=NULL)
		&& strstr(rom, ".jpg")==NULL && strstr(rom, ".JPG")==NULL)
		return true;
	else
		return false;
}


void launch_genp_emu(char *rom)
{
	if(!net_used_ignore()) return;
	if(stat(genp_self, &s3)<0)
	{
		dialog_ret=0;
		cellMsgDialogOpen2( type_dialog_ok, "To play Genesis+ GX games you must install the latest version of GENESIS Emulator for the PS3\xE2\x84\xA2", dialog_fun2, (void*)0x0000aaab, NULL );
		wait_dialog();
		return;
	}
	char* launchargv[2];
	memset(launchargv, 0, sizeof(launchargv));

	int len = strlen(rom);
	launchargv[0] = (char*)malloc(len + 1); strcpy(launchargv[0], rom);
	launchargv[1] = (char*)malloc(len + 1); strcpy(launchargv[1], rom);
	unload_modules();
	sys_game_process_exitspawn2((char*)genp_self, (const char**)launchargv, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_512K);
}


void launch_fceu_emu(char *rom)
{
	if(!net_used_ignore()) return;
	if(stat(fceu_self, &s3)<0)
	{
		dialog_ret=0;
		cellMsgDialogOpen2( type_dialog_ok, "To play NES/FCE Ultra games you must install the latest version of FCEU Emulator for the PS3\xE2\x84\xA2", dialog_fun2, (void*)0x0000aaab, NULL );
		wait_dialog();
		return;
	}
	char* launchargv[2];
	memset(launchargv, 0, sizeof(launchargv));

	int len = strlen(rom);
	launchargv[0] = (char*)malloc(len + 1); strcpy(launchargv[0], rom);
	launchargv[1] = (char*)malloc(len + 1); strcpy(launchargv[1], rom);
	unload_modules();
	sys_game_process_exitspawn2((char*)fceu_self, (const char**)launchargv, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_512K);
}

void launch_vba_emu(char *rom)
{
	if(!net_used_ignore()) return;
	if(stat(vba_self, &s3)<0)
	{
		dialog_ret=0;
		cellMsgDialogOpen2( type_dialog_ok, "To play GameBoy/Advanced games you must install the latest version of VBA Emulator for the PS3\xE2\x84\xA2", dialog_fun2, (void*)0x0000aaab, NULL );
		wait_dialog();
		return;
	}
	char* launchargv[2];
	memset(launchargv, 0, sizeof(launchargv));

	int len = strlen(rom);
	launchargv[0] = (char*)malloc(len + 1); strcpy(launchargv[0], rom);
	launchargv[1] = (char*)malloc(len + 1); strcpy(launchargv[1], rom);
	unload_modules();
	sys_game_process_exitspawn2((char*)vba_self, (const char**)launchargv, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_512K);
}

void get_game_flags(int _game_sel)
{
	menu_list[_game_sel].user=0;
	if(stat(menu_list[_game_sel].path, &s3)<0) return;
	FILE *flist; 
	char game_opts[512];
	char tmpid[9];
	sprintf(game_opts, "%s/PS3_GAME/PS3GAME.INI", menu_list[_game_sel].path);

	flist = fopen(game_opts, "rb");
	if ( flist != NULL )
	{
		fread((char*) &tmpid, 8, 1, flist);
		if(strstr(tmpid, GAME_INI_VER)!=NULL)
		{
			fseek(flist, 8, SEEK_SET);
			fread((char*) &menu_list[_game_sel].user, sizeof(menu_list[_game_sel].user), 1, flist);
			fclose(flist);
			goto quit_with_ok;
		}
		fclose(flist);
	}

quit_with_ok:
	sprintf(game_opts, "%s/PS3_GAME/PS3DATA.BIN", menu_list[_game_sel].path);
	if(stat(game_opts, &s3)>=0) {
		remove(game_opts);
		menu_list[_game_sel].user|= IS_BDMIRROR | IS_USB;
	}
	return;
}

int set_game_flags(int _game_sel)
{
	char game_opts[512];
	sprintf(game_opts, "%s/PS3_GAME", menu_list[_game_sel].path);
	if(stat(game_opts, &s3)<0 || strstr(game_opts, "/dev_bdvd")!=NULL || strstr(game_opts, "/pvd_usb")!=NULL) return 0;
	sprintf(game_opts, "%s/PS3_GAME/PS3GAME.INI", menu_list[_game_sel].path);
	FILE *flist; 
	remove(game_opts);
	flist = fopen(game_opts, "wb");
	if ( flist != NULL )
	{
		fwrite( GAME_INI_VER, 8, 1, flist);
		fwrite( (char*) &menu_list[_game_sel].user, sizeof(menu_list[_game_sel].user), 1, flist);
		fclose(flist);
	}
	return 1;

}

/****************************************************/
/* FTP SECTION                                      */
/****************************************************/


void ftp_handler(CellFtpServiceEvent event, void * /*data*/, size_t /*datalen*/)
{

	switch(event)
	{

	case CELL_FTP_SERVICE_EVENT_SHUTDOWN:
	case CELL_FTP_SERVICE_EVENT_FATAL:
	case CELL_FTP_SERVICE_EVENT_STOPPED:
		ftp_service=0;
		ftp_clients=0;
		break;
	case CELL_FTP_SERVICE_EVENT_CLIENT_CONNECTED:
		ftp_clients++;
		break;

	case CELL_FTP_SERVICE_EVENT_CLIENT_DISCONNECTED:
		ftp_clients--;
		break;

	default:
		break;
	}
}

void ftp_on()
{
	if(ftp_service) return;
		
	if(cellFtpServiceRegisterHandler(ftp_handler)>=0)
	{
		cellFtpServiceStart();
		ftp_service=1;
	}
}

void ftp_off()
{
	if(!ftp_service) return;
		uint64_t result;
		cellFtpServiceStop(&result);
		cellFtpServiceUnregisterHandler();
		ftp_service=0;
		ftp_clients=0;

}
/*
#if (CELL_SDK_VERSION>0x210001)

#else

void ftp_on()
{
	ftp_service=0;
}
void ftp_off()
{
	ftp_service=0;
}
#endif
*/

#define SOCKET_BUF_SIZE_RCV	(384 * 1024)
#define SOCKET_BUF_SIZE_SND	(  1 * 1024)
#define REQUEST_GET2	"\r\n\r\n"
// command: DIR GET INF
//ret = network_com("GET", "10.20.2.208", 11222, "/", "/dev_hdd0/GAMES/down_test.bin", 3);


void net_folder_copy(char *path, char *path_new, char *path_name)
{

	FILE *fp;
	del_temp(app_temp);
	int n=0, foundslash=0, t_files, t_folders;
	uint64_t copy_folder_bytes=0;
	uint64_t global_folder_bytes=0;
	struct CellFsStat sN;
	char net_host_file[512], net_host_file2[512];//, tempname2[512];

	char net_path_bare[512], title[512], date2[10], timeF[8], type[1], net_path[512], parent[512];
	char path2[1024], path3[1024], path4[1024], path_relative[512];

	char temp[3], length[128], tempname2[128]; 
	char cpath[1024], cpath2[1024], tempname[1024];
	int chost=0; int pl=0, pr=0;

	int len;
	int ret;

	int optval;
	unsigned int temp_a;

	char sizestr[32]="0";
	char string1n[1024];
	int seconds2=0;

	struct sockaddr_in sin;
	struct hostent *hp;

	FILE *fid;

	sprintf(net_host_file2, "%s", path); net_host_file2[10]=0;

	sprintf(net_host_file,"%s%s", app_usrdir, net_host_file2);

	fp = fopen ( net_host_file, "r" );
	if ( fp != NULL ) 

	{ 
		temp[0]=0x2e; temp[1]=0x2e; temp[2]=0;
		chost=path[9]-0x30;
		initConsole();
		sprintf(path2, "%s/", path);


//		host_list[chost].host
//		host_list[chost].port


	if ((unsigned int)-1 != (temp_a = inet_addr(host_list[chost].host))) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = temp_a;
	}
	else {
		if (NULL == (hp = gethostbyname(host_list[chost].host))) {
//			printf("unknown host %s, %d\n", host_list[chost].host, sys_net_h_errno);
			goto termination2X;
		}
		sin.sin_family = hp->h_addrtype;
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	}
	sin.sin_port = htons(host_list[chost].port);

	socket_handle = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_handle < 0) {
		goto termination2X;
	} 

	optval = SOCKET_BUF_SIZE_RCV;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	optval = SOCKET_BUF_SIZE_SND;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
	if (ret < 0) {
//		DPrintf("setsockopt() failed (errno=%d)\n", sys_net_errno);
		goto termination2X;
	}

	ret = connect(socket_handle, (struct sockaddr *)&sin, sizeof(sin));
	if (ret < 0) {
//		DPrintf("connect() failed (errno=%d)\n", sys_net_errno);
		goto termination2X;
	}

		copy_folder_bytes=0x00ULL; t_files=0; t_folders=0;
		while (fscanf(fp,"%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%s\n", net_path_bare, title, length, timeF, date2, type, parent, tempname2)>=7) {

			sprintf(net_path, "%s%s", net_host_file2, net_path_bare);
			net_path[strlen(path2)]=0;
//		sprintf(string1n, "[%s] [%s] [%s]", path_new); //, (double) (copy_global_bytes/1024.0f/1024.0f) 
//		dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, string1n, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();

			if((strstr(path_new, "/ps3_home/video")!=NULL && strcmp(net_path, path2)==0 && (strstr(title, ".avi")!=NULL || strstr(title, ".AVI")!=NULL || strstr(title, ".m2ts")!=NULL || strstr(title, ".M2TS")!=NULL || strstr(title, ".mts")!=NULL || strstr(title, ".MTS")!=NULL || strstr(title, ".m2t")!=NULL || strstr(title, ".M2T")!=NULL || strstr(title, ".divx")!=NULL || strstr(title, ".DIVX")!=NULL || strstr(title, ".mpg")!=NULL || strstr(title, ".MPG")!=NULL || strstr(title, ".mpeg")!=NULL || strstr(title, ".MPEG")!=NULL || strstr(title, ".mp4")!=NULL || strstr(title, ".MP4")!=NULL || strstr(title, ".vob")!=NULL || strstr(title, ".VOB")!=NULL || strstr(title, ".wmv")!=NULL || strstr(title, ".WMV")!=NULL || strstr(title, ".ts")!=NULL || strstr(title, ".TS")!=NULL || strstr(title, ".mov")!=NULL || strstr(title, ".MOV")!=NULL) )) 
			{
				temp[0]=0x2e; temp[1]=0x2e; temp[2]=0;
				if(strcmp(temp, title)!=0) { copy_folder_bytes+=strtoull(length, NULL, 10); t_files++;}
					else  t_folders++;
			}

			if((strstr(path_new, "/ps3_home/music")!=NULL && strcmp(net_path, path2)==0 && (strstr(title, ".mp3")!=NULL || strstr(title, ".MP3")!=NULL || strstr(title, ".wav")!=NULL || strstr(title, ".WAV")!=NULL || strstr(title, ".aac")!=NULL || strstr(title, ".AAC")!=NULL) )) 
			{
				temp[0]=0x2e; temp[1]=0x2e; temp[2]=0;
				if(strcmp(temp, title)!=0) { copy_folder_bytes+=strtoull(length, NULL, 10); t_files++;}
					else  t_folders++;
			}

			if((strstr(path_new, "/ps3_home/photo")!=NULL && strcmp(net_path, path2)==0 && (strstr(title, ".jpg")!=NULL || strstr(title, ".JPG")!=NULL || strstr(title, ".jpeg")!=NULL || strstr(title, ".JPEG")!=NULL || strstr(title, ".png")!=NULL || strstr(title, ".PNG")!=NULL) )) 
			{
				temp[0]=0x2e; temp[1]=0x2e; temp[2]=0;
				if(strcmp(temp, title)!=0) { copy_folder_bytes+=strtoull(length, NULL, 10); t_files++;}
					else  t_folders++;
			}

			if((strstr(path_new, "/ps3_home")==NULL && strcmp(net_path, path2)==0)) 
			{
				temp[0]=0x2e; temp[1]=0x2e; temp[2]=0;
				if(strcmp(temp, title)!=0) { copy_folder_bytes+=strtoull(length, NULL, 10); t_files++;}
					else  t_folders++;
			}

		}
		fclose(fp);

//		sprintf(string1n, "Copying network folder (%i files in %i folders) from [%s], please wait!\n\n                                     Press [O] twice to abort.", t_files, t_folders, host_list[chost].host); //, (double) (copy_global_bytes/1024.0f/1024.0f) 
//		dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, string1n, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();

		if(copy_folder_bytes<1) goto termination2X;
		fp = fopen ( net_host_file, "r" );


//	if(copy_folder_bytes>1048576)
	{
		sprintf(string1n, "Copying network folder (%i files in %i folders) from [%s], please wait!", t_files, t_folders, host_list[chost].host); //, (double) (copy_global_bytes/1024.0f/1024.0f) 
//		dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, string1n, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
//		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); cellDbgFontPrintf( 0.20f, 0.45f, 0.8f, 0xc0c0c0c0, string1); cellDbgFontDrawGcm();
		dialog_ret=0;
		cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL	|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF	|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE	|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE, string1n,	NULL,	NULL,	NULL);
		flip();
	}
	dialog_ret=0;
	time_start= time(NULL);
	lastINC2=0;lastINC=0;

		while (fscanf(fp,"%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%s\n", net_path_bare, title, length, timeF, date2, type, parent, tempname2)>=7) {

			global_device_bytes=0x00ULL;

			sprintf(net_path, "%s%s", net_host_file2, net_path_bare);
			net_path[strlen(path2)]=0;

//			sprintf(path4, "%s%s%s", net_host_file2, parent, type);
//			sprintf(path3, "%s/0", path);

//			if(strcmp(path3, path4)==0)
//			{
//				strncpy(list[*max ].path, net_path, strlen(net_path)-1); list[*max ].path[strlen(net_path)-1]=0;
//			} //subfolder

			if(strcmp(net_path, path2)==0 && strstr(path_new, "/ps3_home")==NULL)
			{
				sprintf(path_relative, "/");
				sprintf(tempname, "%s%s", net_host_file2, net_path_bare);

				if(strcmp(tempname, path2)!=0)
				{
					pl=strlen(tempname);
					pr=strlen(path2);
					for(n=pr;n<pl;n++)
					{
						path_relative[n-pr+1]=tempname[n];
						path_relative[n-pr+2]=0;
					}

				}

				sprintf(path3, "%s/%s%s", path_new, path_name, path_relative);

//				sprintf(tempname, "[%s]=[%s] Net: [%s]\n\nLocal: [%s]\n\nRel: %s\n\nNet_path: [%s]", temp, title, net_path_bare, path3, path_relative, net_path);	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, tempname, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();

				temp[0]=0x2e; temp[1]=0x2e; temp[2]=0;
				if(strcmp(temp, title)==0) {
					foundslash=0; pl=strlen(path3);
					for(n=0;n<pl;n++)
					{
						tempname[n]=path3[n];
						tempname[n+1]=0;
						if(path3[n]==0x2F && cellFsStat(tempname, &sN)<0) 
						{	
							mkdir(tempname, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(tempname, 0777);
						}
					}
				}
			}

			if(strcmp(net_path, path2)==0 && strcmp(temp, title)!=0)
			{

				if(strtoull(length, NULL, 10)<1) goto skip_zero_files;

				sprintf(path4, "%s%s%s", net_host_file2, net_path_bare, title);

				pl=strlen(path4);
				for(n=11;n<pl;n++) {cpath[n-11]=path4[n]; cpath[n-10]=0;}
				sprintf(cpath2, "/%s",  cpath); //host_list[chost].root,
				sprintf(cpath, "%s%s" , path3, title);

			if((strstr(path_new, "/ps3_home")!=NULL && strcmp(net_path, path2)==0 && (strstr(title, ".avi")!=NULL || strstr(title, ".AVI")!=NULL || strstr(title, ".m2ts")!=NULL || strstr(title, ".M2TS")!=NULL || strstr(title, ".mts")!=NULL || strstr(title, ".MTS")!=NULL || strstr(title, ".m2t")!=NULL || strstr(title, ".M2T")!=NULL || strstr(title, ".divx")!=NULL || strstr(title, ".DIVX")!=NULL || strstr(title, ".mpg")!=NULL || strstr(title, ".MPG")!=NULL || strstr(title, ".mpeg")!=NULL || strstr(title, ".MPEG")!=NULL || strstr(title, ".mp4")!=NULL || strstr(title, ".MP4")!=NULL || strstr(title, ".vob")!=NULL || strstr(title, ".VOB")!=NULL || strstr(title, ".wmv")!=NULL || strstr(title, ".WMV")!=NULL || strstr(title, ".ts")!=NULL || strstr(title, ".TS")!=NULL || strstr(title, ".mov")!=NULL || strstr(title, ".MOV")!=NULL || strstr(title, ".mp3")!=NULL || strstr(title, ".MP3")!=NULL || strstr(title, ".wav")!=NULL || strstr(title, ".WAV")!=NULL || strstr(title, ".aac")!=NULL || strstr(title, ".AAC")!=NULL || strstr(title, ".jpg")!=NULL || strstr(title, ".JPG")!=NULL || strstr(title, ".jpeg")!=NULL || strstr(title, ".JPEG")!=NULL || strstr(title, ".png")!=NULL || strstr(title, ".PNG")!=NULL) )) 
				{
					sprintf(cpath, "%s/%s", app_temp, title); 
				}
			else
				if(strstr(path_new, "/ps3_home")!=NULL) goto skip_zero_files;
	remove(cpath);
	fid = fopen(cpath, "wb");

	sprintf(get_cmd, "GET %s%s", cpath2, REQUEST_GET2);
	len = strlen(get_cmd);
	ret = send(socket_handle, get_cmd, len, 0);

	if (ret < 0 || ret!=len) {
		goto terminationX;
	}

	ret = recv(socket_handle, buf, sizeof(buf), 0);
	for(int i=10;i<64;i++) //get content length
	{
		sizestr[i-10]=buf[i];
		if(buf[i]==0) break;
		sizestr[i-9]=0;
	}

	copy_global_bytes=strtoull(sizestr, NULL, 10);
	
	if(copy_global_bytes<1 || copy_folder_bytes<1) goto terminationX;

	while (1) {

	int seconds= (int) (time(NULL)-time_start);
	int eta=0;
	if(global_folder_bytes>0 && seconds>0) eta=(int) ((copy_folder_bytes-global_folder_bytes)/(global_folder_bytes/seconds));
	lastINC3=0;

	if( ( ((int)(global_folder_bytes*100ULL/copy_folder_bytes)) - lastINC2)>0)
	{
		lastINC2=(int) (global_folder_bytes*100ULL / copy_folder_bytes);
		if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}
		if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
	}

//	if(copy_folder_bytes>1048576)
		{
		if(lastINC3>3 || (time(NULL)-seconds2)>0 || global_device_bytes==0) 
		{
//			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
//			draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x101010ff);
/*
			sprintf(string1,"Transferred %1.2f of %1.2f KB\n\nRemaining  : %imin %2.2isec\nElapsed    : %imin %2.2isec",((double) global_device_bytes)/(1024.0),((double) copy_global_bytes)/(1024.0), (eta/60), eta % 60, (seconds/60), seconds % 60);
			cellDbgFontPrintf( 0.07f, 0.1f, 1.2f, 0xc0c0c0c0, string1);

			sprintf(string1,"Source     : host://%s:%i%s\nDestination: %s", server_name, server_port, net_file, save_path);
			cellDbgFontPrintf( 0.07f, 0.3f, 1.2f, 0xc0c0c0c0, string1);

			sprintf(string1,"Speed      : %3.2f KB/s (%2.3f Mbit/s)", ((double) global_device_bytes)/seconds/1024.0, ((double) global_device_bytes*8)/seconds/1000.0/1000.0);
			cellDbgFontPrintf( 0.07f, 0.5f, 1.2f, 0xc0c0c0c0, string1);

			cellDbgFontDrawGcm();
*/
			sprintf(string1n,"Transferred %.2f of %.2f MB. Remaining: %imin %2.2isec",((double) global_folder_bytes)/(1024.0)/(1024.0),((double) copy_folder_bytes)/(1024.0)/(1024.0), (eta/60), eta % 60);
			cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, string1n);

			seconds2= (int) (time(NULL));
			flip(); 
		}
	}

		ret = recv(socket_handle, buf, sizeof(buf), 0);

		if (ret < 0) {
			goto terminationX;
		}

		if (ret > 0)
		{
			global_device_bytes+=ret;
			global_folder_bytes+=ret;
			fwrite(buf, ret, 1, fid);
		}


	if (ret == 0 || global_device_bytes>=copy_global_bytes) { break; }

	pad_read();
	if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) || dialog_ret==3) break;
		//cellSysutilCheckCallback();
	}  //while

//	DPrintf("Sending WAIT command... ");
	if(copy_global_bytes<1048576) sys_timer_usleep(250*1000);
	sprintf(get_cmd, "WAIT ! %s", REQUEST_GET2);
	len = strlen(get_cmd);
	ret = send(socket_handle, get_cmd, len, 0);
//	DPrintf("%i\n", len);

terminationX:

	fclose(fid);

skip_zero_files:
	len=0;
	//if(copy_global_bytes>1048576) {cellMsgDialogAbort(); flip();}

//				list[*max].type=strtol(type, NULL, 10);
//				sprintf(list[*max ].path, "%s/%s", path, title);

//				temp[0]=0x2e; temp[1]=0x2e; temp[2]=0;
//				if(strcmp(temp, title)==0)
//				{
//					sprintf(list[*max ].path, "%s", path);
//					char *pch=list[*max ].path;
//					char *pathpos=strrchr(pch,'/');	int lastO=pathpos-pch;
//					list[*max ].path[lastO]=0; 
//				}

//				sprintf(list[*max ].entry, "__%i%s", list[*max].type, title);
//				list[*max].size=strtoull(length, NULL, 10);

			}
			pad_read();
			if (new_pad & BUTTON_CIRCLE) 
			{
				new_pad=0;
				break;
			}		
		} //while

/*		cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Press [O] to continue!");
		cellDbgFontDrawGcm(); 

		while(1)
		{
			pad_read();
			if (new_pad & BUTTON_CIRCLE) 
			{
				new_pad=0;
				break;
			}
		} */

termination2X:

//	DPrintf("Sent STOP command!\n");
	sprintf(get_cmd, "STOP ! %s", REQUEST_GET2);
	len = strlen(get_cmd);
	ret = send(socket_handle, get_cmd, len, 0);

//	DPrintf("Aborting socket...\n");
	sys_net_abort_socket (socket_handle, SYS_NET_ABORT_STRICT_CHECK);

		ret = shutdown(socket_handle, SHUT_RDWR);
		if (ret < 0) {
			DPrintf("shutdown() failed (errno=%d)\n", sys_net_errno);
		}
		socketclose(socket_handle);

	fclose(fp);

	cellMsgDialogAbort();
	sys_timer_usleep(1000000);
	flip();

/*
 	DPrintf("\nPress [O] to continue...");
		while(1)
		{
			pad_read();
			flip();
			if (new_pad & BUTTON_CIRCLE) 
			{
				new_pad=0;
				break;
			}
		} */


	if(strstr(path_new, "/ps3_home")!=NULL) 
				{
	DIR  *dir;
	char tr[512];
	for (n=0;n<256;n++ )
	{
	dir=opendir (app_temp);
	if(!dir) return;

	while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;

		if(!(entry->d_type & DT_DIR))
			{

			sprintf(tr, "%s", entry->d_name);
			cellDbgFontDrawGcm(); 
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			set_texture( text_FMS, 1920, 48); display_img(0, 47, 1920, 60, 1920, 48, -0.15f, 1920, 48);	display_img(0, 952, 1920, 76, 1920, 48, -0.15f, 1920, 48);time ( &rawtime );	timeinfo = localtime ( &rawtime );	cellDbgFontPrintf( 0.83f, 0.89f, 0.7f ,0xc0a0a0a0, "%02d/%02d/%04d\n %s:%02d:%02d ", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, tmhour(timeinfo->tm_hour), timeinfo->tm_min, timeinfo->tm_sec);
			set_texture( text_bmpIC, 320, 320);  display_img(800, 200, 320, 176, 320, 176, 0.0f, 320, 320);
			if(	strstr(path_new, "/ps3_home/video")!=NULL ) {
				cellDbgFontPrintf( 0.35f, 0.45f, 0.8f, 0xc0c0c0c0, "Adding files to video library...\n\nPlease wait!\n\n[%s]",tr); cellDbgFontDrawGcm();
				flip();
		 		video_export(tr, path_name, 0);
			}
			else if(	strstr(path_new, "/ps3_home/music")!=NULL ) 
				{
					cellDbgFontPrintf( 0.35f, 0.45f, 0.8f, 0xc0c0c0c0, "Adding files to music library...\n\nPlease wait!\n\n[%s]",tr); cellDbgFontDrawGcm();
					flip();
			 		music_export(tr, path_name, 0);
				}
			else if(	strstr(path_new, "/ps3_home/photo")!=NULL ) 
				{
					cellDbgFontPrintf( 0.35f, 0.45f, 0.8f, 0xc0c0c0c0, "Adding files to photo library...\n\nPlease wait!\n\n[%s]",tr); cellDbgFontDrawGcm();
					flip();
			 		photo_export(tr, path_name, 0);
				}
			sprintf(tr, "%s/%s", app_temp, entry->d_name);
			if(stat(tr, &s3)>=0) remove(tr);
			}
		}
	closedir(dir);
	}

	ret = video_finalize();
	ret = music_finalize();
	ret = photo_finalize();
	del_temp(app_temp);
				}



		termConsole();

	}
}

int network_put(char *command, char *server_name, int server_port, char *net_file, char *save_path, int show_progress )
{

	int len;
	int ret;

	int optval;
	unsigned int temp_a;

//	char sizestr[32]="0";
	char string1[1024];
	int seconds2=0;
	global_device_bytes=0x00ULL;

	struct sockaddr_in sin;
	struct hostent *hp;
//	time_t time_start;

	if(show_progress==1) 
	{	
		sprintf(string1, "Establishing server connection, please wait!\n\nContacting %s...", server_name);
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		cellDbgFontPrintf( 0.28f, 0.45f, 0.8f, 0xc0c0c0c0, string1);
		cellDbgFontDrawGcm();
		flip();
	}


	if ((unsigned int)-1 != (temp_a = inet_addr(server_name))) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = temp_a;
	}
	else {
		if (NULL == (hp = gethostbyname(server_name))) {
//			printf("unknown host %s, %d\n", server_name, sys_net_h_errno);
			return (-1);
		}
		sin.sin_family = hp->h_addrtype;
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	}
	sin.sin_port = htons(server_port);

	socket_handle = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_handle < 0) {
		return (-1);
	} 

		int fs;
		uint64_t fsiz = 0, msiz = 0;
		uint64_t chunk = 320 * 1024;
		uint64_t readb=0;
//		char w[chunk];

		if(cellFsOpen(save_path, CELL_FS_O_RDONLY, &fs, NULL, 0)!=CELL_FS_SUCCEEDED)  goto termination_PUT;
		cellFsLseek(fs, 0, CELL_FS_SEEK_END, &msiz);
		cellFsClose(fs);

		copy_global_bytes=msiz;
		if(msiz<chunk && msiz>0) chunk=msiz;

		cellFsOpen(save_path, CELL_FS_O_RDONLY, &fs, NULL, 0);
		cellFsLseek(fs, 0, CELL_FS_SEEK_SET, NULL);

//	FILE *fid = fopen(save_path, "rb");
//	if(fid==NULL) goto termination_PUT;
//	fseek(fid, 0, SEEK_END);
//	copy_global_bytes=ftell(fid);
//	fseek(fid, 0, SEEK_SET);

	optval = 1 * 1024;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	if (ret < 0) goto termination_PUT;

	optval = 320 * 1024;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
	if (ret < 0) goto termination_PUT;

	ret = connect(socket_handle, (struct sockaddr *)&sin, sizeof(sin));
	if (ret < 0) goto termination_PUT;

	sprintf(get_cmd, "%s %s%s", command, net_file, REQUEST_GET2);

	len = strlen(get_cmd);
	ret = send(socket_handle, get_cmd, len, 0);
	if (ret < 0) {
		goto termination_PUT;
	}
	if (ret != len) {
		goto termination_PUT;
	}

	ret = recv(socket_handle, buf, sizeof(buf), 0);
	/*
	for(int i=10;i<64;i++) //get content length
	{
		sizestr[i-10]=buf[i];
		if(buf[i]==0) break;
		sizestr[i-9]=0;
	}
	*/

	if(show_progress!=0 && copy_global_bytes>1048576){
		dialog_ret=0; sprintf(string1, "Copying file to network host [%s], please wait!", server_name); //, (double) (copy_global_bytes/1024.0f/1024.0f) 
		cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL	|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF	|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE	|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE, string1,	NULL,	NULL,	NULL);

		flip();
	}

	lastINC2=0;lastINC=0;

	global_device_bytes=0x00ULL;

	time_start = time(NULL);
	while(fsiz < msiz) {

	int seconds= (int) (time(NULL)-time_start);
	int eta=(int) ((copy_global_bytes-global_device_bytes)/(global_device_bytes/seconds));
	lastINC3=0;

	if( ( ((int)(global_device_bytes*100ULL/copy_global_bytes)) - lastINC2)>0)
	{
		lastINC2=(int) (global_device_bytes*100ULL / copy_global_bytes);
		if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}

		if(show_progress!=0){
			if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
		}
	}

	if(show_progress!=0 && copy_global_bytes>1048576){
		if(lastINC3>3 || (time(NULL)-seconds2)>0 || global_device_bytes==0) 
		{
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x101010ff);

			sprintf(string1,"Transferred %.2f of %.2f MB. Remaining: %imin %2.2isec",((double) global_device_bytes)/(1024.0)/(1024.0),((double) copy_global_bytes)/(1024.0)/(1024.0), (eta/60), eta % 60);
			cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, string1);

			seconds2= (int) (time(NULL));
			flip(); 
		}
	}


		if((fsiz+chunk) > msiz)	chunk = (msiz-fsiz);
		if(cellFsRead(fs, (void *)buf, chunk, &readb)!=CELL_FS_SUCCEEDED) goto termination_PUT;
		fsiz = fsiz + readb;
		global_device_bytes=fsiz;

		if(send(socket_handle, buf, readb, 0)<0)	goto termination_PUT;

	if (readb == 0 || global_device_bytes>=copy_global_bytes) { break; }

	pad_read();
	if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) ) break;
	}  //while



termination_PUT:
	cellFsClose(fs);

	sys_net_abort_socket (socket_handle, SYS_NET_ABORT_STRICT_CHECK);
	ret = shutdown(socket_handle, SHUT_RDWR);
	socketclose(socket_handle);
	if(show_progress!=0){ cellMsgDialogAbort();  } //flip();
	return (0);
}


int network_del(char *command, char *server_name, int server_port, char *net_file, char *save_path, int show_progress )
{

	int len;
	int ret;
	(void) show_progress;
	(void) save_path;
	int optval;
	unsigned int temp_a;

	struct sockaddr_in sin;
	struct hostent *hp;


	if ((unsigned int)-1 != (temp_a = inet_addr(server_name))) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = temp_a;
	}
	else {
		if (NULL == (hp = gethostbyname(server_name))) {
//			printf("unknown host %s, %d\n", server_name, sys_net_h_errno);
			return (-1);
		}
		sin.sin_family = hp->h_addrtype;
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	}
	sin.sin_port = htons(server_port);

	socket_handle = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_handle < 0) {
		return (-1);
	} 

	optval = 1 * 1024;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	if (ret < 0) goto termination_DEL;

	optval = 1 * 1024;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
	if (ret < 0) goto termination_DEL;

	ret = connect(socket_handle, (struct sockaddr *)&sin, sizeof(sin));
	if (ret < 0) goto termination_DEL;

	sprintf(get_cmd, "%s %s%s", command, net_file, REQUEST_GET2);

	len = strlen(get_cmd);
	ret = send(socket_handle, get_cmd, len, 0);
	if (ret < 0) {
		goto termination_DEL;
	}
	if (ret != len) {
		goto termination_DEL;
	}
	ret = recv(socket_handle, buf, sizeof(buf), 0);

termination_DEL:

	sys_net_abort_socket (socket_handle, SYS_NET_ABORT_STRICT_CHECK);
	ret = shutdown(socket_handle, SHUT_RDWR);
	socketclose(socket_handle);
	return (0);
}

int read_folder(char *path)
{
   DIR  *dir;

	if(abort_copy==1) return -1;
    dir=opendir (path); 
    if(!dir) return -1;


   while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;

		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;

		if((entry->d_type & DT_DIR))
			{
			
				char *d1f= (char *) malloc(512);
				num_directories++;

				if(!d1f) {closedir (dir);abort_copy=2;return -1;}
				sprintf(d1f,"%s/%s", path, entry->d_name);
				sprintf(f_files[max_f_files].path, "%s/", d1f);
				f_files[max_f_files].size=0;
				max_f_files++;
				if(max_f_files>=MAX_F_FILES) break;

				read_folder(d1f);
				free(d1f);
				if(abort_copy) break;
			}
		else
			{
				char *f= (char *) malloc(512);
				struct stat s;
				off64_t size=0LL;

				if(!f) {abort_copy=2;closedir (dir);return -1;}
				sprintf(f, "%s/%s", path, entry->d_name);
				if(stat(f, &s)<0) {abort_copy=3;if(f) free(f);break;}
				size= s.st_size;

				sprintf(f_files[max_f_files].path, "%s", f);
				f_files[max_f_files].size=size;
				max_f_files++;
				if(max_f_files>=MAX_F_FILES) break;

			 	file_counter++;
				global_device_bytes+=size;
				if(f) free(f);
			
			if(abort_copy) break;

			}

		}

	closedir (dir);

return 0;
}


int network_com(char *command, char *server_name, int server_port, char *net_file, char *save_path, int show_progress )
{


//	char *server_name;

	int len;
	int ret;

	int optval;
	unsigned int temp_a;

	char sizestr[32]="0";
	char string1[1024];
	int seconds2=0;
	global_device_bytes=0x00ULL;

	struct sockaddr_in sin;
	struct hostent *hp;
//	time_t time_start;

	if(show_progress==1) 
	{	
		if(strstr(command, "GET!")!=NULL)
			sprintf(string1, "Refreshing network directory structure, please wait!\n\nContacting %s...", server_name);
		else
			sprintf(string1, "Establishing server connection, please wait!\n\nContacting %s...", server_name);
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		cellDbgFontPrintf( 0.28f, 0.45f, 0.8f, 0xc0c0c0c0, string1);
		cellDbgFontDrawGcm();
		flip();
	}


	if ((unsigned int)-1 != (temp_a = inet_addr(server_name))) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = temp_a;
	}
	else {
		if (NULL == (hp = gethostbyname(server_name))) {
//			printf("unknown host %s, %d\n", server_name, sys_net_h_errno);
			return (-1);
		}
		sin.sin_family = hp->h_addrtype;
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	}
	sin.sin_port = htons(server_port);

	socket_handle = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_handle < 0) {
		return (-1);
	} 


	remove(save_path);
	FILE *fid = fopen(save_path, "wb");

	optval = SOCKET_BUF_SIZE_RCV;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	optval = SOCKET_BUF_SIZE_SND;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
	if (ret < 0) {
		DPrintf("setsockopt() failed (errno=%d)\n", sys_net_errno);
		goto termination;
	}


//	optval = 15; //set socket send/receive/connect timeout to 15 seconds
//	ret = setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, &optval, sizeof(optval));
//	ret = setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, &optval, sizeof(optval));


	ret = connect(socket_handle, (struct sockaddr *)&sin, sizeof(sin));
	if (ret < 0) {
		DPrintf("connect() failed (errno=%d)\n", sys_net_errno);
		goto termination;
	}

//	strcpy(get_cmd, command);
//	strcat(get_cmd, " ");
//	strcat(get_cmd, net_file);
//	strcat(get_cmd, REQUEST_GET2);
	sprintf(get_cmd, "%s %s%s", command, net_file, REQUEST_GET2);

	len = strlen(get_cmd);
	ret = send(socket_handle, get_cmd, len, 0);
	if (ret < 0) {
		goto termination;
	}
	if (ret != len) {
		goto termination;
	}

	ret = recv(socket_handle, buf, sizeof(buf), 0);
	for(int i=10;i<64;i++) //get content length
	{
		sizestr[i-10]=buf[i];
		if(buf[i]==0) break;
		sizestr[i-9]=0;
	}
	copy_global_bytes=strtoull(sizestr, NULL, 10);
	if(copy_global_bytes<1) goto termination;
	

	if(show_progress!=0 && copy_global_bytes>1048576){
		sprintf(string1, "Copying network file from [%s], please wait!", server_name); //, (double) (copy_global_bytes/1024.0f/1024.0f) 
//		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); cellDbgFontPrintf( 0.20f, 0.45f, 0.8f, 0xc0c0c0c0, string1); cellDbgFontDrawGcm();

		cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL	|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF	|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE	|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE, string1,	NULL,	NULL,	NULL);

		flip();
	}

	lastINC2=0;lastINC=0;

	global_device_bytes=0x00ULL;

	time_start = time(NULL);
//	len = 0;
	while (1) {

	int seconds= (int) (time(NULL)-time_start);
	int eta=(int) ((copy_global_bytes-global_device_bytes)/(global_device_bytes/seconds));
	lastINC3=0;

	if( ( ((int)(global_device_bytes*100ULL/copy_global_bytes)) - lastINC2)>0)
	{
		lastINC2=(int) (global_device_bytes*100ULL / copy_global_bytes);
		if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}

		if(show_progress!=0){
			if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
		}

	}

	if(show_progress!=0 && copy_global_bytes>1048576){
		if(lastINC3>3 || (time(NULL)-seconds2)>0 || global_device_bytes==0) 
		{
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x101010ff);
/*
			sprintf(string1,"Transferred %1.2f of %1.2f KB\n\nRemaining  : %imin %2.2isec\nElapsed    : %imin %2.2isec",((double) global_device_bytes)/(1024.0),((double) copy_global_bytes)/(1024.0), (eta/60), eta % 60, (seconds/60), seconds % 60);
			cellDbgFontPrintf( 0.07f, 0.1f, 1.2f, 0xc0c0c0c0, string1);

			sprintf(string1,"Source     : host://%s:%i%s\nDestination: %s", server_name, server_port, net_file, save_path);
			cellDbgFontPrintf( 0.07f, 0.3f, 1.2f, 0xc0c0c0c0, string1);

			sprintf(string1,"Speed      : %3.2f KB/s (%2.3f Mbit/s)", ((double) global_device_bytes)/seconds/1024.0, ((double) global_device_bytes*8)/seconds/1000.0/1000.0);
			cellDbgFontPrintf( 0.07f, 0.5f, 1.2f, 0xc0c0c0c0, string1);

			cellDbgFontDrawGcm();
*/
			sprintf(string1,"Transferred %.2f of %.2f MB. Remaining: %imin %2.2isec",((double) global_device_bytes)/(1024.0)/(1024.0),((double) copy_global_bytes)/(1024.0)/(1024.0), (eta/60), eta % 60);
			cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, string1);

			seconds2= (int) (time(NULL));
			flip(); 
		}
	}

		ret = recv(socket_handle, buf, sizeof(buf), 0);

		if (ret < 0) {
			fclose(fid);
			goto termination;
		}

		if (ret > 0)
		{
			global_device_bytes+=ret;
			fwrite(buf, ret, 1, fid);
		}


	if (ret == 0 || global_device_bytes>=copy_global_bytes) { break; }

	pad_read();
	if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) ) break;
	}  //while



termination:
	fclose(fid);

//termination2:
	sys_net_abort_socket (socket_handle, SYS_NET_ABORT_STRICT_CHECK);
	ret = shutdown(socket_handle, SHUT_RDWR);
	if (ret < 0) {
		DPrintf("shutdown() failed (errno=%d)\n", sys_net_errno);
	}
	socketclose(socket_handle);
//	sys_timer_usleep(250*1000);
	if(show_progress!=0){ cellMsgDialogAbort();  } //flip();
	return (0);
}


void net_folder_copy_put(char *path, char *path_new, char *path_name)
{
	file_counter=0;
	global_device_bytes=0;
	num_directories=0;
	max_f_files=0;

	dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Preparing data and network connection, please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
	flipc(60);
	read_folder((char*) path);
	int e=0;
	if(max_f_files>0)
	{

	int len;
	int ret;
	int optval;
	unsigned int temp_a;

	struct sockaddr_in sin;
	struct hostent *hp;

	int fs;
	uint64_t fsiz = 0, msiz = 0;
	uint64_t chunk = 1 * 1024 * 1024; //320 * 1024;
	uint64_t readb=0;
//	char w[chunk];
	int seconds2=0;

	copy_global_bytes=global_device_bytes;
	global_device_bytes=0x00ULL;
	lastINC=0, lastINC3=0, lastINC2=0;

							char cpath2[1024];
							int chost=0;
							chost=path_new[9]-0x30;
							if(strlen(path_new)>11)
								sprintf(cpath2, "/%s", path_new+11);
							else
								cpath2[0]=0x00;

	char server_name[64];
//	char net_folder[512];
	int server_port;

	sprintf(server_name, "%s", host_list[chost].host);
	server_port=host_list[chost].port;

	if ((unsigned int)-1 != (temp_a = inet_addr(server_name))) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = temp_a;
	}
	else {
		if (NULL == (hp = gethostbyname(server_name))) {
			cellMsgDialogAbort();
			return;
		}
		sin.sin_family = hp->h_addrtype;
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	}
	sin.sin_port = htons(server_port);

	socket_handle = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_handle < 0) {
		cellMsgDialogAbort();
		return;
	} 

	optval = 1 * 1024;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	if (ret < 0) goto termination_FC;

	optval = 320 * 1024;
	ret = setsockopt(socket_handle, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
	if (ret < 0) goto termination_FC;

	ret = connect(socket_handle, (struct sockaddr *)&sin, sizeof(sin));
	if (ret < 0) goto termination_FC;

		cellMsgDialogAbort(); flip();

		char string1[256];
		sprintf(string1, "Copying local folder (%i files in %i folders) to network host [%s], please wait!", file_counter, num_directories, host_list[chost].host);
		cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL	|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF	|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE	|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE, string1,	NULL,	NULL,	NULL);
		flipc(60);
//		dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, string1, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();

		seconds2= (int) time(NULL);	
		time_start = time(NULL);

		for(e=0; e<max_f_files; e++)
		{

			if(f_files[e].path[strlen(f_files[e].path)-1]!=0x2f) // file
				sprintf(get_cmd, "PUT-%.f %s/%s%s%s", (double)f_files[e].size, cpath2, path_name, f_files[e].path+strlen(path), REQUEST_GET2);
			else
				sprintf(get_cmd, "PUT %s/%s%s%s", cpath2, path_name, f_files[e].path+strlen(path), REQUEST_GET2);
			len = strlen(get_cmd);
			ret = send(socket_handle, get_cmd, len, 0);
			if (ret < 0) goto termination_FC;
			if (ret != len) goto termination_FC;
			ret = recv(socket_handle, buf, 1 * 1024, 0);

			if(f_files[e].path[strlen(f_files[e].path)-1]!=0x2f) // file
			{

				if(cellFsOpen(f_files[e].path, CELL_FS_O_RDONLY, &fs, NULL, 0)!=CELL_FS_SUCCEEDED)  goto termination_SEND;
				msiz=f_files[e].size;
				cellFsLseek(fs, 0, CELL_FS_SEEK_SET, NULL);
				fsiz=0;

	while(fsiz < msiz) {


	int seconds= (int) (time(NULL)-time_start);
	int eta=(int) ((copy_global_bytes-global_device_bytes)/(global_device_bytes/seconds));
	lastINC3=0;

	if( ( ((int)(global_device_bytes*100ULL/copy_global_bytes)) - lastINC2)>0)
	{
		lastINC2=(int) (global_device_bytes*100ULL / copy_global_bytes);
		if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}
		if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
	}

//	if(copy_global_bytes>1048576)
	{
		if(lastINC3>3 || (time(NULL)-seconds2)>0 || global_device_bytes==0) 
		{
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x101010ff);

			sprintf(string1,"Transferred %.2f of %.2f MB. Remaining: %imin %2.2isec",((double) global_device_bytes)/(1024.0)/(1024.0),((double) copy_global_bytes)/(1024.0)/(1024.0), (eta/60), eta % 60);
			cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, string1);

			seconds2= (int) (time(NULL));
			flip(); 
		}
	}

//		if((fsiz+chunk) > msiz)	{ chunk = (msiz-fsiz);} else chunk = 320*1024;
//		if(cellFsRead(fs, (void *)w, chunk, &readb)!=CELL_FS_SUCCEEDED) {cellFsClose(fs); goto termination_SEND;}
		cellFsRead(fs, (void *)buf, chunk, &readb);
		fsiz = fsiz + readb;

		if (readb == 0 || global_device_bytes>=copy_global_bytes) { break; }
		if(send(socket_handle, buf, readb, 0)<0)	goto termination_SEND;
		global_device_bytes+=readb;
		if(fsiz>=msiz) break;


	pad_read();
	if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) ) break;
	}  //while

				cellFsClose(fs);
				if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) ) break;
			}

			if(e!=(max_f_files-1))
			{
				sprintf(get_cmd, "WAIT ! %s", REQUEST_GET2);
				len = strlen(get_cmd);
				sys_timer_usleep(500*1000);
				ret = send(socket_handle, get_cmd, len, 0);
				if(ret<0) break;
				sys_timer_usleep(333*1000);
			}
			else
			{
				sys_timer_usleep(500*1000);
				sprintf(get_cmd, "STOP ! %s", REQUEST_GET2);
				len = strlen(get_cmd);
				ret = send(socket_handle, get_cmd, len, 0);
			}
		}

termination_SEND:
termination_FC:
	cellMsgDialogAbort();
	sys_net_abort_socket (socket_handle, SYS_NET_ABORT_STRICT_CHECK);
	ret = shutdown(socket_handle, SHUT_RDWR);
	socketclose(socket_handle);

	network_com((char*)"GET!",(char*)host_list[chost].host, host_list[chost].port, (char*)"/", (char*) host_list[chost].name, 1);

	} //there are files to copy
	else
		cellMsgDialogAbort();

}

#define HTTP_POOL_SIZE      (64 * 1024)
int download_file(const char *http_file, const char *save_path, int show_progress)
{

	if(net_avail<0) return 0;
//	time_t time_start;
	time_start= time(NULL);
	char string1[256];
	global_device_bytes=0x00UL;
	int seconds2=0;
	if(show_progress==1) sprintf(string1, "Downloading update data, please wait!");
	if(show_progress==2) sprintf(string1, "Downloading cover, please wait!");
	if(show_progress==3) sprintf(string1, "Downloading file, please wait!");
	if(show_progress==4) sprintf(string1, "Downloading theme, please wait!");
	if(show_progress!=0){
	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	cellDbgFontPrintf( 0.3f, 0.45f, 0.8f, 0xc0c0c0c0, string1);
	cellDbgFontDrawGcm();
	}

if(show_progress>0 && show_progress!=3) {
cellMsgDialogOpen2(
	CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL
	|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE
	|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF
	|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE
	|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE,
	string1,
	NULL,
	NULL,
	NULL);

flip();
	}

lastINC2=0;lastINC=0;
int ret = 0;
static char buffer[10240];

CellHttpClientId client = 0;
CellHttpTransId trans = 0;
CellHttpUri uri;
int code = 0;
uint64_t length = 0;
size_t localRecv = -1;
size_t poolSize = 0;
void *uriPool = NULL;
void *httpPool = NULL;

FILE *fid;

global_device_bytes=0;
remove(save_path);

httpPool = malloc(HTTP_POOL_SIZE);
if (!httpPool) {
goto quit;
}

if (cellHttpInit(httpPool, (size_t)HTTP_POOL_SIZE) < 0) {
goto quit;
}

if (cellHttpCreateClient(&client) < 0) {
goto quit;
}

cellHttpClientSetConnTimeout(client, 5 * 1000000);

if (cellHttpUtilParseUri(NULL, http_file, NULL, 0, &poolSize) < 0) {
goto quit;
}

uriPool = malloc(poolSize);
if (!uriPool) {
goto quit;
}

if (cellHttpUtilParseUri(&uri, http_file, uriPool, poolSize, NULL) < 0) {
goto quit;
}

if (cellHttpCreateTransaction(&trans, client, CELL_HTTP_METHOD_GET, &uri) < 0) {
goto quit;
}

free(uriPool);
uriPool = NULL;

if (cellHttpSendRequest(trans, NULL, 0, NULL) < 0) {goto quit;}
if (cellHttpResponseGetStatusCode(trans, &code) < 0) {goto quit;}
if (code == 404 || code == 403) {ret=-1; goto quit;}
if (cellHttpResponseGetContentLength(trans, &length) < 0) {goto quit;}

copy_global_bytes=length;

fid = fopen(save_path, "wb");
if (!fid) {goto quit;}

memset(buffer, 0x00, sizeof(buffer));
while (localRecv != 0) {
if (cellHttpRecvResponse(trans, buffer, sizeof(buffer) - 1, &localRecv) < 0) {
	fclose(fid);
	goto quit;
}
if (localRecv == 0) break;
global_device_bytes+=localRecv;
fwrite(buffer, localRecv, 1, fid);


	int seconds= (int) (time(NULL)-time_start);
	int eta=(copy_global_bytes-global_device_bytes)/(global_device_bytes/seconds);
	lastINC3=0;

if( ( ((int)(global_device_bytes*100ULL/copy_global_bytes)) - lastINC2)>0)
{
	lastINC2=(int) (global_device_bytes*100ULL / copy_global_bytes);
	if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}

	if(show_progress!=0 && show_progress!=3){
		if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
	}

}
	if(show_progress!=0){

if(lastINC3>3 || (time(NULL)-seconds2)>1 || (show_progress==3 && www_running)) 
{	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	if(show_progress==3 && www_running)
	{
		if(cover_mode==8)
		{
			draw_xmb_clock(xmb_clock, 0);
			set_texture( text_bmp, 1920, 1080); 
			display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
		}
		else
		{
			set_texture( text_bmpUPSR, 1920, 1080); 
					BoffX--;
					if(BoffX<= -3840) BoffX=0;
					if(BoffX>= -1920) {
						display_img((int)BoffX, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					}

					display_img(1920+(int)BoffX, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);

					if(BoffX<= -1920) {
						display_img(3840+(int)BoffX, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					}
		}
		sprintf(string1,"\nDownloaded %.1f of %.3f MB. Remaining: %imin %2.2isec (/\\ to cancel)\nSave path: %s",((double) global_device_bytes)/(1024.0)/(1024.0f),((double) copy_global_bytes)/(1024.0)/(1024.0f), (eta/60), eta % 60, save_path);
		cellDbgFontPrintf( 0.07f, 0.9f, 1.0f, 0xc0c0c0c0, string1);
	}
	else
	{
		draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x101010ff);
		sprintf(string1,"Downloaded %1.2f of %1.2f KB. Remaining: %imin %2.2isec\n\nPress /\\ to cancel download",((double) global_device_bytes)/(1024.0),((double) copy_global_bytes)/(1024.0), (eta/60), eta % 60);
		cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
	}
	cellDbgFontDrawGcm();
	seconds2= (int) (time(NULL));
	sprintf(string1,"Downloaded %1.2f of %1.2f KB. Remaining: %imin %2.2isec",((double) global_device_bytes)/(1024.0),((double) copy_global_bytes)/(1024.0), (eta/60), eta % 60);
	if(show_progress!=3)
		cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, string1);
	flip(); 
}

	}

	pad_read();
	if ( ( new_pad & BUTTON_TRIANGLE) || ( old_pad & BUTTON_TRIANGLE))
		{
		ret = 2;
		new_pad=0; old_pad=0;
		fclose(fid);
		remove(save_path);
		goto quit;
		}
}

fclose(fid);
ret = 1;

quit:
	if (trans) cellHttpDestroyTransaction(trans);
	if (client) cellHttpDestroyClient(client);
	cellHttpEnd();
	if (httpPool) free(httpPool);
	if (uriPool) free(uriPool);
if(show_progress==3){
		if(stat(download_dir, &s3)>=0)
			sprintf(current_right_pane, "%s", download_dir);
		else
			sprintf(current_right_pane, "%s/DOWNLOADS", app_usrdir);
		state_read=3;
}

	if(show_progress!=0){ cellMsgDialogAbort(); flip(); }
	if(global_device_bytes==0) remove(save_path);
return ret;
}

int download_file_th(const char *http_file, const char *save_path, int params)
{
	(void) params;

//	if(net_avail<0) return 0;

	int ret = 0;
	u32 received_bytes=0;
	unsigned char *buffer=NULL;
	buffer = (unsigned char *) malloc(16384);

	CellHttpClientId client = 0;
	CellHttpTransId trans = 0;
	CellHttpUri uri;
	int code = 0;
	uint64_t length = 0;
	size_t localRecv = -1;
	size_t poolSize = 0;
	void *uriPool = NULL;
	void *httpPool = NULL;

	FILE *fid;
	received_bytes=0;
	remove(save_path);

	httpPool = malloc(HTTP_POOL_SIZE);
	if (!httpPool) goto quit_th;
	if (cellHttpInit(httpPool, (size_t)HTTP_POOL_SIZE) < 0) {goto quit_th;}
	if (cellHttpCreateClient(&client) < 0) {goto quit_th;}
	cellHttpClientSetConnTimeout(client, 5 * 1000000);
	if (cellHttpUtilParseUri(NULL, http_file, NULL, 0, &poolSize) < 0) {goto quit_th;}
	uriPool = malloc(poolSize);if (!uriPool) {goto quit_th;}
	if (cellHttpUtilParseUri(&uri, http_file, uriPool, poolSize, NULL) < 0) {goto quit_th;}

	if (cellHttpCreateTransaction(&trans, client, CELL_HTTP_METHOD_GET, &uri) < 0) {goto quit_th;}
	free(uriPool);
	uriPool = NULL;

	if (cellHttpSendRequest(trans, NULL, 0, NULL) < 0) {goto quit_th;}
	if (cellHttpResponseGetStatusCode(trans, &code) < 0) {goto quit_th;}
	if (code == 404 || code == 403) {ret=-1; goto quit_th;}
	if (cellHttpResponseGetContentLength(trans, &length) < 0) {goto quit_th;}


	fid = fopen(save_path, "wb");
	if (!fid) {goto quit_th;}
	http_active=true;
	memset(buffer, 0x00, sizeof(buffer));
	while (localRecv != 0) {
		if (cellHttpRecvResponse(trans, buffer, sizeof(buffer) - 1, &localRecv) < 0) {
			fclose(fid);
			goto quit_th;
		}
		if (localRecv == 0) break;
		if (localRecv > 0) 
		{
			fwrite(buffer, localRecv, 1, fid);
			received_bytes+=localRecv;
		}
	}
	fclose(fid);
	ret = 1;

quit_th:
	if(buffer) free(buffer);
	if (trans) cellHttpDestroyTransaction(trans);
	if (client) cellHttpDestroyClient(client);
	cellHttpEnd();
	if (httpPool) free(httpPool);
	if (uriPool) free(uriPool);

	if(received_bytes==0) remove(save_path);
	http_active=false;
return ret;
}

void download_cover(char *title_id, char *name)
{
	if(strlen(title_id)!=9 || download_covers!=1 || net_avail<0) return; //strstr (title_id,"NO_ID")!=NULL || strstr (title_id,"AVCHD")!=NULL) return;

	net_avail=cellNetCtlGetInfo(16, &net_info);
	if(net_avail<0) return;


	char covers_url[128];
	char string1x[128];
	if(strstr(name, ".jpg")!=NULL || strstr(name, ".JPG")!=NULL || strstr(name, ".jpeg")!=NULL || strstr(name, ".JPEG")!=NULL)
		sprintf(string1x, "%s/%s.JPG", covers_dir, title_id);
	else
		sprintf(string1x, "%s/%s.PNG", covers_dir, title_id);

	sprintf(covers_url, "%s/covers_jpg/%s.JPG", url_base, title_id);
	u8 is_in_queue=0;
	for(int n=0;n<downloads_max; n++)
		if(!strcmp(downloads[downloads_max].url, covers_url)){is_in_queue=1; break;}

	if(!is_in_queue)
	{
		if(downloads_max>=MAX_DOWN_LIST) downloads_max=0;
		sprintf(downloads[downloads_max].url, "%s", covers_url);
		sprintf(downloads[downloads_max].local, "%s", string1x);
		downloads[downloads_max].status=1;
		downloads_max++;
		if(downloads_max>=MAX_DOWN_LIST) downloads_max=0;
	}
	//if(download_file(covers_url, string1x, 2)==0) net_avail=-1;
}

//u64 ret2 = syscall_837(837, (u64)"CELL_FS_IOS:USB_MASS_STORAGE000", (u64)"CELL_FS_UDF", (u64)"/dev_usb006", 0, 0, 0, 0, 0);
static uint64_t syscall_837(const char *device, const char *format, const char *point, u32 a, u32 b, u32 c, void *buffer, u32 len)
{
	system_call_8(837, (u64)device, (u64)format, (u64)point, a, b, c, (u64)buffer, len);
	return_to_user_prog(uint64_t);
}

static uint64_t syscall_838(const char *device)
{
	system_call_1(838, (u64)device);
	return_to_user_prog(uint64_t);
}

/*******************/
/* Control Console */
#if (CELL_SDK_VERSION>0x210001)
CellConsoleInputProcessorResult _cellConsoleResetBDVD
				(unsigned int uiConnection,
				const char *pcInput,
				void *pvDummy,
				int iContinuation) {

	(void) pvDummy; (void) iContinuation; (void)uiConnection; (void)iContinuation; (void) pcInput;
    cellConsolePrintf(uiConnection, "\nResetting dev_bdvd...\n");
	syscall_838("/dev_bdvd");
	syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_UDF", "/dev_bdvd", 0, 1, 0, 0, 0);
    cellConsolePrintf(uiConnection, "Done!\n\n");
	
    return CELL_CONSOLE_INPUT_PROCESSED;
}


CellConsoleInputProcessorResult _cellConsoleFTP
				(unsigned int uiConnection,
				const char *pcInput,
				void *pvDummy,
				int iContinuation) {

	(void) pvDummy; (void) iContinuation;
	char ftp_command[32];

      if (sscanf(pcInput, "%*s %s", ftp_command)==1) {

  		  if(strcmp(ftp_command, "start")==0){
              cellConsolePrintf(uiConnection, "Status: Starting FTP server...\n");
			  ftp_on();
			  if(ftp_service==0)
	              cellConsolePrintf(uiConnection, "Status: Failed to start FTP server!\n");
			  else
 	              cellConsolePrintf(uiConnection, "Status: FTP server started!\n");
		  }

  		  if(strstr(ftp_command, "stop")!=NULL){
              cellConsolePrintf(uiConnection, "Status: Stopping FTP server...\n");
			  ftp_off();
			  if(ftp_service==1)
	              cellConsolePrintf(uiConnection, "Status: Failed to start FTP server!\n");
			  else
 	              cellConsolePrintf(uiConnection, "Status: FTP server stopped!\n");
		  }

  		  if(strstr(ftp_command, "restart")!=NULL){
              cellConsolePrintf(uiConnection, "Status: Stopping FTP server...\n");
			  ftp_off();
              cellConsolePrintf(uiConnection, "Status: Starting FTP server...\n");
			  ftp_on();
			  if(ftp_service==0)
	              cellConsolePrintf(uiConnection, "Status: Failed to restart FTP server!\n");
			  else
 	              cellConsolePrintf(uiConnection, "Status: FTP server restarted!\n");
		  }

      } else {
		  if(ftp_service)
              cellConsolePrintf(uiConnection, "Status: FTP server is running\n\n");
		  else
              cellConsolePrintf(uiConnection, "Status: FTP server is stopped\n\n");

           cellConsolePrintf(uiConnection, "Usage:\nftp\nftp start\nftp stop\nftp restart\n");
      }
      cellConsolePrintf(uiConnection, "\nmultiMAN> ");
      return CELL_CONSOLE_INPUT_PROCESSED;
}

CellConsoleInputProcessorResult _cellConsolePeek
				(unsigned int uiConnection,
				const char *pcInput,
				void *pvDummy,
				int iContinuation) {

	(void) pvDummy; (void) iContinuation;
	char peek_addr[32]; peek_addr[0]=0;
	uint64_t peekA=0;

      if (sscanf(pcInput, "%*s %s", peek_addr)==1) {
		  peekA=strtoull(peek_addr, NULL, 16)+0x8000000000000000ULL;
  		  if(peekA>0x80000000007ffff8ULL) return CELL_CONSOLE_INPUT_PROCESSED;
			cellConsolePrintf(uiConnection, "peek(0x80000000%08X): 0x%08X%08X\n", peekA, (peekq(peekA)>>32), peekq(peekA)); 
      } else {
            cellConsolePrintf(uiConnection, 
				"Usage: peek <u32 address>\n\npeek 2f8011\n\n");
      }
      cellConsolePrintf(uiConnection, "\nmultiMAN> ");
      return CELL_CONSOLE_INPUT_PROCESSED;
}

CellConsoleInputProcessorResult _cellConsolePeekL
				(unsigned int uiConnection,
				const char *pcInput,
				void *pvDummy,
				int iContinuation) {

	(void) pvDummy; (void) iContinuation;
	char peek_addr[32]; peek_addr[0]=0;
	uint64_t peekA=0; int n=0;

      if (sscanf(pcInput, "%*s %s", peek_addr)==1) {
		for(n=0; n<256; n+=8)
		  {
		peekA=strtoull(peek_addr, NULL, 16)+0x8000000000000000ULL + (uint64_t) n;
		if(peekA>0x80000000007ffff8ULL) break;
		cellConsolePrintf(uiConnection, "peek(0x80000000%08X): 0x%08X%08X\n", peekA, (peekq(peekA)>>32), peekq(peekA)); 
		  }

      } else {
            cellConsolePrintf(uiConnection, 
				"Usage: peekl <u32 address>\n\npeek 2f8011\n\n");
      }
      cellConsolePrintf(uiConnection, "\nmultiMAN> ");
      return CELL_CONSOLE_INPUT_PROCESSED;
}

CellConsoleInputProcessorResult _cellConsolePoke
				(unsigned int uiConnection,
				const char *pcInput,
				void *pvDummy,
				int iContinuation) {

	(void) pvDummy; (void) iContinuation;
	char peek_addr[32]; peek_addr[0]=0;
	char poke_val[32]; poke_val[0]=0;
	uint64_t peekA=0, pokeA=0;

      if (sscanf(pcInput, "%*s %s %s", peek_addr, poke_val)==2) {
			peekA=strtoull(peek_addr, NULL, 16)+0x8000000000000000ULL;
			if(peekA>0x80000000007ffff8ULL) return CELL_CONSOLE_INPUT_PROCESSED;
			cellConsolePrintf(uiConnection, "peek(0x80000000%08X): 0x%08X%08X\n", peekA, (peekq(peekA)>>32), peekq(peekA)); 

  			pokeA=strtoull(poke_val, NULL, 16);
			pokeq(peekA, pokeA);
			peekA=strtoull(peek_addr, NULL, 16)+0x8000000000000000ULL;
			cellConsolePrintf(uiConnection, "poke(0x80000000%08X)= 0x%08X%08X\n", peekA, (peekq(peekA)>>32), peekq(peekA)); 

      } else {
            cellConsolePrintf(uiConnection, 
				"Usage: poke <u32 address> <u64 value>\n\npoke 2f8011 1020304050607080\n\n");
      }
      cellConsolePrintf(uiConnection, "\nmultiMAN> ");
      return CELL_CONSOLE_INPUT_PROCESSED;
}

CellConsoleInputProcessorResult _cellConsoleQuitXMB
				(unsigned int uiConnection,
				const char *pcInput,
				void *pvDummy,
				int iContinuation) {

	(void) pvDummy; (void) iContinuation; (void)uiConnection; (void)iContinuation; (void) pcInput;
    cellConsolePrintf(uiConnection, "\nShutting down multiMAN...\n");
	unload_modules();
    cellConsolePrintf(uiConnection, "Done!\n\n");
	sys_process_exit(1);
	
    return CELL_CONSOLE_INPUT_PROCESSED;
}

CellConsoleInputProcessorResult _cellConsoleSS
				(unsigned int uiConnection,
				const char *pcInput,
				void *pvDummy,
				int iContinuation) {

	(void) pvDummy; (void) iContinuation; (void)uiConnection; (void)iContinuation; (void) pcInput;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	char video_mem[64], scm[128];
	sprintf(video_mem, "/dev_hdd0/%04d%02d%02d-%02d%02d%02d-SCREENSHOT.RAW", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	sprintf(scm, "\nSaving screenshot to: [%s]...\n", video_mem);
    cellConsolePrintf(uiConnection, scm);
	FILE *fpA;
	remove(video_mem);
	fpA = fopen ( video_mem, "wb" );
	uint64_t c_pos=0;
//	fwrite(color_base_addr, video_buffer, 1, fpA);
	for(c_pos=0;c_pos<video_buffer;c_pos+=4){
		fwrite((uint8_t*)(color_base_addr)+c_pos+1, 3, 1, fpA);
	}
	fclose(fpA);
    cellConsolePrintf(uiConnection, "Done!\n\nmultiMAN> ");

	
    return CELL_CONSOLE_INPUT_PROCESSED;
}

CellConsoleInputProcessorResult _cellConsoleRESTART
				(unsigned int uiConnection,
				const char *pcInput,
				void *pvDummy,
				int iContinuation) {

	(void) pvDummy; (void) iContinuation; (void)uiConnection; (void)iContinuation; (void) pcInput;
    cellConsolePrintf(uiConnection, "\nShutting down multiMAN...\n");

/*		char* launchenvp[2];
		char* launchargv[0x100];
		memset(launchenvp, 0, sizeof(launchenvp));
		memset(launchargv, 0, sizeof(launchargv));

		launchenvp[0] = (char*)malloc(0x440);
		snprintf(launchenvp[0], 0x440, "%s", mmbin);
*/
//	cellFsChmod(mmbin, 0170777ULL);
//		sysProcessExitSpawn2(mmbin, (const char**)launchargv, (const char**)launchenvp, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);


/*	cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP);
#define NP_POOL_SIZE (128*1024)
	uint8_t np_pool[NP_POOL_SIZE];
	sceNpInit(NP_POOL_SIZE, np_pool);
	SceNpDrmKey* k_licensee = NULL;
//	system_call_1(36, (uint32_t) app_usrdir);
	sceNpDrmProcessExitSpawn2(k_licensee, (char*)"/dev_hdd0/game/BLES80608/USRDIR/mm.ppu.self", (const char**)launchargv, (const char**)launchenvp, NULL, 0, 1001, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
	sceNpTerm();
	cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
	*/
//	sprintf(filename, "/dev_hdd0/game/%s/RELOAD", app_usrdir);
//	system_call_1(36, (uint32_t) filename);
//	syscall_mount(filename, mount_bdvd);
	cellConsolePrintf(uiConnection, "Trying to restart...\n");
//	debug_print=102030;

	//to_reboot=1;
	char reload_self[128];
	sprintf(reload_self, "%s/RELOAD.SELF", app_usrdir);
	if(stat(reload_self,&s3)>=0)
	{
		cellConsolePrintf(uiConnection, "Re-spawning [RELOAD.SELF]...\n");
		unload_modules();
		sys_game_process_exitspawn2((char*) reload_self, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);	
	}


    return CELL_CONSOLE_INPUT_PROCESSED;
}

CellConsoleInputProcessorResult _cellConsoleDebug
				(unsigned int uiConnection,
				const char *pcInput,
				void *pvDummy,
				int iContinuation) {

	(void) pvDummy; (void) iContinuation; (void)uiConnection; (void)iContinuation; (void) pcInput;



	debug_print=uiConnection;
    cellConsolePrintf(uiConnection, "Debug enabled.\n\nmultiMAN> ");
	dialog_ret=3;
	
    return CELL_CONSOLE_INPUT_PROCESSED;
}
#endif
/****************************************************/
/* MODULES SECTION                                  */
/****************************************************/

static int unload_mod=0;

static int load_modules()
{
	int ret;

	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
	if (ret != CELL_OK) return ret;
	else unload_mod|=1;
	
	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_PNGDEC);
	if(ret != CELL_OK) return ret;
	else unload_mod|=2;
		
	ret = cellSysmoduleLoadModule( CELL_SYSMODULE_IO );
	if (ret != CELL_OK) return ret;
	else unload_mod|=4;

	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_JPGDEC);
	cellSysmoduleLoadModule( CELL_SYSMODULE_VDEC_MPEG2 );
	cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
	CellScreenShotSetParam  screenshot_param = {0, 0, 0, 0};
	screenshot_param.photo_title = "multiMAN";
	screenshot_param.game_title = "multiMAN Screenshots";
	screenshot_param.game_comment = current_version_NULL;
	cellScreenShotSetParameter (&screenshot_param);
	cellScreenShotEnable();
//	cellScreenShotSetOverlayImage(app_homedir,	(char*) "ICON0.PNG", 50, 50);

	//cellSysmoduleInitialize();
	ret = cellSysmoduleLoadModule( CELL_SYSMODULE_AUDIO );
	
//	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_VIDEO_EXPORT);
//	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_MUSIC_EXPORT);
//	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_PHOTO_EXPORT);

	cellMouseInit (1);
	cellKbInit(1);
	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_NET);
	if (ret < 0) return ret; else unload_mod|=16;

	cellNetCtlInit();
	net_available = (cellSysmoduleLoadModule(CELL_SYSMODULE_HTTP)==CELL_OK);
	net_available = (sys_net_initialize_network()==0) | net_available;


#if (CELL_SDK_VERSION>0x210001)
	cellConsoleInit();
	cellConsoleNetworkInitialize(); 		
	cellConsoleNetworkServerInit(8080);

cellConsoleInputProcessorAdd("resetbd", 
				"Unmount/mount dev_bdvd",
			      "", 0,
			      _cellConsoleResetBDVD);

cellConsoleInputProcessorAdd("debug", 
				"Enable disable debug messages",
			      "on|off", 0,
			      _cellConsoleDebug);

cellConsoleInputProcessorAdd("poke", 
				"Write value to LV2 memory",
			      "<u32 address> <u64 value>", 0,
			      _cellConsolePoke);

cellConsoleInputProcessorAdd("peekl", 
				"Read 256 values from LV2 memory",
			      "<u32 address>", 0,
			      _cellConsolePeekL);

cellConsoleInputProcessorAdd("peek", 
				"Read value from LV2 memory",
			      "<u32 address>", 0,
			      _cellConsolePeek);
	                                     

cellConsoleInputProcessorAdd("ftp", 
				"Manage FTP server",
			      "start|stop|restart", 0,
			      _cellConsoleFTP);

cellConsoleInputProcessorAdd("quit", 
				"Quit multiMAN and exit to XMB",
			      "", 0,
			      _cellConsoleQuitXMB);

cellConsoleInputProcessorAdd("screenshot", 
				"Save current screen as RAW (RGB) image in /dev_hdd0",
			      "", 0,
			      _cellConsoleSS);

cellConsoleInputProcessorAdd("restart", 
				"Restart multiMAN",
			      "", 0,
			      _cellConsoleRESTART);

#endif

	void LoadModules();

		ret = Fonts_LoadModules();
		if ( ret != CELL_OK ) {
			return CELL_OK;
		}
		
		fonts = Fonts_Init();
		if ( fonts ) {
			ret = Fonts_InitLibraryFreeType( &freeType );
			if ( ret == CELL_OK ) {
				ret = Fonts_OpenFonts( freeType, fonts, app_usrdir );
				if ( ret == CELL_OK ) {
					ret = Fonts_CreateRenderer( freeType, 0, &RenderWork.Renderer );
					if ( ret == CELL_OK ) {
//						printf("App:Fonts Initialize All OK!\n");
						
						//ret = Fonts_AttachFont( fonts, FONT_SYSTEM_GOTHIC_JP, &FontBitmaps.Font );
						//Fonts_BindRenderer( &FontBitmaps.Font, &RenderWork.Renderer );
						//FontBitmaps_Init( &FontBitmaps, (CellFont*)0, 24.f, 18.f, 1.0f, 0.1f, 256 );
						
						return CELL_OK;
					}

					Fonts_CloseFonts( fonts );
				}
				Fonts_EndLibrary( freeType );
			}
			Fonts_End();
			fonts = (Fonts_t*)0;
		}

	return ret;
}

void pfs_mode(int _mode)
{
#if (CELL_SDK_VERSION>0x210001)
	if(pfs_enabled && _mode==0)
	{	
		pfs_enabled=0;
		usbdev_uninit();

	}

	if(!pfs_enabled && _mode==1)
	{	
		if(never_used_pfs)
		{	usbdev_init(0);
			PfsmInit(max_usb_volumes);
		}

		usbdev_init(1);
		pfs_enabled=1;
		never_used_pfs=0;
	}

	if(pfs_enabled && _mode==2)
	{	
		pfs_enabled=0;
		usbdev_uninit();
		PfsmUninit();


	}
#else
	(void) _mode;
#endif
}

void delete_entries(t_menu_list *list, int *max, u32 flag)
{
	int n;

	n=0;

	while(n<(*max) )
		{
		if(list[n].flags & flag)
			{
			if((*max) >1)
				{
				list[n].flags=0;
				list[n]=list[(*max) -1];
				(*max) --;
				}
			else  {if((*max) == 1)(*max) --; break;}
			
			}
		else n++;

		}
}

void sort_entries(t_menu_list *list, int *max)
{
	int n,m,o1=0,o2=0;
	int fi= (*max);
	
	for(n=0; n< (fi -1);n++)
		for(m=n+1; m< fi ;m++)
			{
			if(list[n].title[0]=='_') o1=1; else o1=0;
			if(list[m].title[0]=='_') o2=1; else o2=0;
			
			if((strcasecmp(list[n].title+o1, list[m].title+o2)>0  && ((list[n].flags | list[m].flags) & 2048)==0) || 
				((list[m].flags & 2048) && n==0))
				{
				t_menu_list swap;
					swap=list[n];list[n]=list[m];list[m]=swap;
				}
			}

	for(n=0; n< (fi -1);n++)
		for(m=n+1; m< fi ;m++)
			if(strcasecmp(list[n].path, list[m].path)==0 && strstr(list[n].content,"PS3")!=NULL)	sprintf(list[m].title, "%s", "_DELETE");

	n=0;
	while(n<(*max) )
		{
		if(strstr(list[n].title, "_DELETE")!=NULL)
			{
			if((*max) >1)
				{
				list[n].flags=0;
				list[n]=list[(*max) -1];
				(*max) --;
				}
			else  {if((*max) == 1)(*max) --; break;}
			
			}
		else n++;

		}
}

void delete_xmb_member(xmbmem *list, u16 *max, int n)
{
		if((*max) >1)
		{
			list[n]=list[(*max) -1];
			(*max) --;
		}
		else  {if((*max) == 1)(*max) --;}
}


void sort_xmb_col(xmbmem *_xmb, u16 max, int _first)
{
	if((max)<2) return;
	int n,m,o1=0,o2=0;
	u16 fi= (max);
	
	for(n=_first; n< (fi -1);n++)
		for(m=n+1; m< fi ;m++)
			{
			if(_xmb[n].name[0]=='_') o1=1; else o1=0;
			if(_xmb[m].name[0]=='_') o2=1; else o2=0;
			
			if(strcasecmp(_xmb[n].name+o1, _xmb[m].name+o2)>0)// && strstr(_xmb[n].name+o1, _xmb[m].name+o2)==NULL)
				{
				xmbmem swap;
					swap=_xmb[n];_xmb[n]=_xmb[m];_xmb[m]=swap;
				}
			}
}



static int unload_modules()
{
	init_finished=0;
	/*
	if(text_bmp!=NULL)
	{
		u8 *buffer=text_bmp;
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		flip();
		max_ttf_label=0; memset(buffer, 0, 1920*1080*4);
		print_label_ex( 0.5f, 0.50f, 1.0f, 0xffffffff, (char*)"Boo!", 1.04f, 0.0f, 15, 1.0f, 1.0f, 1);
		flush_ttf(buffer, 1920, 1080);
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		set_texture( buffer, 1920, 1080); 
		display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
		setRenderColor();
		flip();
	}*/

	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);flip();
	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);flip();
		

	save_options();
	write_last_state();
	FILE *flist; 
	remove(list_file);
	if(!pfs_enabled)
	{
		flist = fopen(list_file, "wb");
		if(flist!=NULL)
		{
			fwrite((char*) GAME_LIST_VER, 8, 1, flist);
			fwrite((char*) &menu_list, ((max_menu_list+1)*sizeof(t_menu_list)), 1, flist); //556 sizeof(t_menu_list)
			fclose(flist);
		}
	}
	else
	{
		fdevices_old=0;
		sprintf(current_left_pane, "%s", "/");
		sprintf(current_right_pane, "%s", "/");
	}

	reset_xmb_checked();
	sprintf(list_file_state, "%s/XMBS.ALL", app_usrdir);
	remove(list_file_state);
	flist = fopen(list_file_state, "wb");
	if(flist!=NULL)
	{
		fwrite((char*) XMB_COL_VER, 8, 1, flist);
		fwrite((char*) &xmb, sizeof(xmb), 1, flist);
		fclose(flist);
	}


/*	if(xmb[8].size>1)
	{
		sprintf(list_file_state, "%s/XMBS.008", app_usrdir);
		remove(list_file_state);
		flist = fopen(list_file_state, "wb");
		if(flist!=NULL)
		{
			fwrite((char*) XMB_COL_VER, 8, 1, flist);
			fwrite((char*) &xmb[8], sizeof(xmb[8]), 1, flist);
			fclose(flist);
		}
	}
*/
	

	sprintf(list_file_state, "%s/LSTAT.BIN", app_usrdir);
	remove(list_file_state);
	flist = fopen(list_file_state, "wb");
	if(flist!=NULL)
	{
		if(xmb_icon==1 && xmb[xmb_icon].first>6) xmb[xmb_icon].first=1;
		fwrite((char*) GAME_STATE_VER, 8, 1, flist);
		fwrite((char*) &fdevices_old, sizeof(fdevices_old), 1, flist);
		fwrite((char*) &mouseX, sizeof(mouseX), 1, flist);
		fwrite((char*) &mouseY, sizeof(mouseY), 1, flist);
		fwrite((char*) &mp3_volume, sizeof(mp3_volume), 1, flist);
		fwrite((char*) &current_left_pane, sizeof(current_left_pane), 1, flist);
		fwrite((char*) &current_right_pane, sizeof(current_right_pane), 1, flist);
		fwrite((char*) &xmb_icon, sizeof(xmb_icon), 1, flist);
		fwrite((char*) &xmb[xmb_icon].first, sizeof(xmb[xmb_icon].first), 1, flist);
		fclose(flist);
	}

	enable_sc36();
	ftp_off();
	cellPadEnd();

	pfs_mode(2);

	sys_vm_unmap(vm);

	if(unload_mod & 16) cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
	if(unload_mod & 8) cellSysmoduleUnloadModule(CELL_SYSMODULE_GCM_SYS);
	if(unload_mod & 4) cellSysmoduleUnloadModule(CELL_SYSMODULE_IO);
	if(unload_mod & 2) cellSysmoduleUnloadModule(CELL_SYSMODULE_PNGDEC);
					   cellSysmoduleUnloadModule(CELL_SYSMODULE_JPGDEC);
	if(unload_mod & 1) cellSysmoduleUnloadModule(CELL_SYSMODULE_FS);

	cellSysmoduleUnloadModule(CELL_SYSMODULE_VDEC_MPEG2);

	cellSysmoduleUnloadModule( CELL_SYSMODULE_SYSUTIL_SCREENSHOT );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_AUDIO );
	if(ve_initialized) cellSysmoduleUnloadModule( CELL_SYSMODULE_VIDEO_EXPORT );
	if(me_initialized) cellSysmoduleUnloadModule( CELL_SYSMODULE_MUSIC_EXPORT );
	if(pe_initialized) cellSysmoduleUnloadModule( CELL_SYSMODULE_PHOTO_EXPORT );

	//FontBitmaps_End( &FontBitmaps );
	//Fonts_UnbindRenderer( &FontBitmaps.Font );
	//Fonts_DetachFont( &FontBitmaps.Font );
	
	Fonts_CloseFonts( fonts );
	Fonts_DestroyRenderer( &RenderWork.Renderer );
	Fonts_EndLibrary( freeType );
	Fonts_End();
	Fonts_UnloadModules();
	cellMouseEnd();
	cellKbEnd();
	sys_memory_container_destroy( memory_container_web );
	sys_memory_container_destroy( memory_container );

	cellSysmoduleFinalize();

	return 0;
}





/****************************************************/
/* PNG SECTION                                      */
/****************************************************/


typedef struct CtrlMallocArg
{
	u32 mallocCallCounts;

} CtrlMallocArg;


typedef struct CtrlFreeArg
{
	u32 freeCallCounts;

} CtrlFreeArg;

void *png_malloc(u32 size, void * a)
{
    CtrlMallocArg *arg;
	arg = (CtrlMallocArg *) a;
	arg->mallocCallCounts++;
	return malloc(size);
}


static int png_free(void *ptr, void * a)
{
    CtrlFreeArg *arg;
  	arg = (CtrlFreeArg *) a;
  	arg->freeCallCounts++;
	free(ptr);
	return 0;
}

int png_out_mapmem(u8 *buffer, size_t buf_size)
{
	int ret;
	u32 offset;
	ret = cellGcmMapMainMemory(buffer, buf_size, &offset);
	if(CELL_OK != ret ) return ret;
	return 0;
}

int png_w=0, png_h=0, png_w2, png_h2;
int jpg_w=0, jpg_h=0;
int png_w_th=0, png_h_th=0;

/*
typedef struct{
    sys_ppu_thread_t    *ppuThreadId; 
    uint32_t             fileNum;
    usecond_t            sleepTime;
    CellJpgDecCommand   *commandPtr;
} CellJpgTimerThreadArg_t; */

//typedef int32_t(*CellPngDecCbControlStream)(
/*int32_t png_callback(
	CellPngDecStrmInfo *strmInfo,
	CellPngDecStrmParam *strmParam,
	void *cbCtrlStrmArg)
{
	return 0;
}*/

/*
typedef struct CellPngDecDispInfo {
	uint64_t outputFrameWidthByte;
	uint32_t outputFrameHeight;
	uint64_t outputStartXByte;
	uint32_t outputStartY;
	uint64_t outputWidthByte;
	uint32_t outputHeight;
	uint32_t outputBitDepth;
	uint32_t outputComponents;
	uint32_t nextOutputStartY;
	uint32_t scanPassCount;
	void *outputImage;
} CellPngDecDispInfo;


typedef struct CellPngDecDispParam {
	void *nextOutputImage;
} CellPngDecDispParam;
*/

/*

int32_t pngDispCb( CellPngDecDispInfo *dispInfo, CellPngDecDispParam *dispParam, void *cbCtrlDispArg )
{
	(void) cbCtrlDispArg;
	dispParam->nextOutputImage=(u8*)(dispInfo->outputImage) + (dispInfo->outputWidthByte * dispInfo->outputHeight);
//	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
//	set_texture( text_bmp, 1920, 1080); //PIC1.PNG
//	display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//	flip();
	pad_read();

	{

	if  ((new_pad & BUTTON_UP)) 
	{		
		c_opacity_delta=16; dimc=0; dim=1;
		if(cover_mode==8 && xmb[xmb_icon].size>1)
		{
			if(xmb[xmb_icon].first==0) {xmb[xmb_icon].first=xmb[xmb_icon].size-1; xmb_bg_show=0; xmb_bg_counter=200;}
			else xmb_slide_step_y=10;
		}
	}

	else if  ((new_pad & BUTTON_DOWN)) 
	{		

		if(cover_mode==8 && xmb[xmb_icon].size>1)
		{
			if(xmb[xmb_icon].first==xmb[xmb_icon].size-1) {xmb[xmb_icon].first=0;  xmb_bg_show=0; xmb_bg_counter=200;}
			else xmb_slide_step_y=-10;// && xmb[xmb_icon].first<xmb[xmb_icon].size-1) xmb[xmb_icon].first++;
		}
	}

	else if ((new_pad & BUTTON_LEFT))
	{
		if(cover_mode==8 && xmb_icon>1) xmb_slide_step=15;
	}

	else if ((new_pad & BUTTON_RIGHT))
	{
			if(cover_mode==8 && xmb_icon<MAX_XMB_ICONS-1) xmb_slide_step=-15;
	}


	}
	draw_xmb_bare(xmb_icon, 1, 1, 0);
	
	return 0;
}


int load_png_partial(u8 *data, char *name, uint16_t _DW, u32 _lines, u16 *_image_id)
{

	int  ret_file, ret, ok=-1;
	
	CellPngDecMainHandle        mHandle;
	CellPngDecSubHandle         sHandle;

	CellPngDecThreadInParam 	InParam;
	CellPngDecThreadOutParam 	OutParam;

	CellPngDecOpnParam			opnParam;
	CellPngDecExtInfo			extInfo;

	CellPngDecSrc 		        src; 
	CellPngDecOpnInfo 	        opnInfo;
	CellPngDecInfo 		        info;

	CellPngDecDataOutInfo 	    dOutInfo;
	CellPngDecDataCtrlParam     dCtrlParam;

	CellPngDecInParam 	        inParam;
	CellPngDecExtInParam		extInParam;

	CellPngDecOutParam 	        outParam;
	CellPngDecExtOutParam		extoutParam;

	CtrlMallocArg               MallocArg;
	CtrlFreeArg                 FreeArg;

//	CellPngDecCbCtrlStrm		pngCallback;
//	pngCallback.cbCtrlStrmFunc= png_callback;
//	pngCallback.cbCtrlStrmArg = NULL;

	CellPngDecCbCtrlDisp		pngDisp;
	pngDisp.cbCtrlDispFunc	=	pngDispCb;
	pngDisp.cbCtrlDispArg	=	&_image_id;

	CellPngDecDispParam			pngDispInfo;
	pngDispInfo.nextOutputImage	=	data;

	int ret_png=-1;

	opnParam.selectChunk	  = 0; //no extra chunks needed

	InParam.spuThreadEnable   = CELL_PNGDEC_SPU_THREAD_DISABLE;
	InParam.ppuThreadPriority = 3071;
	InParam.spuThreadPriority = 255;
	InParam.cbCtrlMallocFunc  = png_malloc;
	InParam.cbCtrlMallocArg   = &MallocArg;
	InParam.cbCtrlFreeFunc    = png_free;
	InParam.cbCtrlFreeArg     = &FreeArg;

	extInParam.bufferMode	  =  CELL_PNGDEC_LINE_MODE;
	extInParam.spuMode		  =  CELL_PNGDEC_TRYRECEIVE_EVENT;
	extInParam.outputCounts   = _lines;
	
	//extoutParam.outputWidthByte
	//extoutParam.outputHeight


	ret_png= ret= cellPngDecCreate(&mHandle, &InParam, &OutParam);
	
//	memset(data, 0x00, sizeof(data)); //(DISPLAY_WIDTH * DISPLAY_HEIGHT * 4)

	png_w= png_h= 0;

	if(ret_png == CELL_OK)
		{
		
			memset(&src, 0, sizeof(CellPngDecSrc));
			src.srcSelect     = CELL_PNGDEC_FILE;
			src.fileName      = name;

			src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_DISABLE;
			
//			ret_file=ret = cellPngDecOpen(mHandle, &sHandle, &src, &opnInfo);
//			ret_file=ret = cellPngDecExtOpen(mHandle, &sHandle, &src, &opnInfo, &pngCallback, &opnParam);
			ret_file=ret = cellPngDecExtOpen(mHandle, &sHandle, &src, &opnInfo, NULL, &opnParam);
			
			if(ret == CELL_OK)
				{
//				ret = cellPngDecReadHeader(mHandle, sHandle, &info);
				ret = cellPngDecExtReadHeader(mHandle, sHandle, &info, &extInfo);
				}

			if(ret == CELL_OK && (_DW * info.imageHeight <= 2073600))
				{	
				inParam.commandPtr        = NULL;
				inParam.outputMode        = CELL_PNGDEC_TOP_TO_BOTTOM;
				inParam.outputColorSpace  = CELL_PNGDEC_RGBA;
				inParam.outputBitDepth    = 8;
				inParam.outputPackFlag    = CELL_PNGDEC_1BYTE_PER_1PIXEL;

				if((info.colorSpace == CELL_PNGDEC_GRAYSCALE_ALPHA) || (info.colorSpace == CELL_PNGDEC_RGBA) || (info.chunkInformation & 0x10))
					inParam.outputAlphaSelect = CELL_PNGDEC_STREAM_ALPHA;
				else
					inParam.outputAlphaSelect = CELL_PNGDEC_FIX_ALPHA;

				inParam.outputColorAlpha  = 0xff;


//				ret = cellPngDecSetParameter(mHandle, sHandle, &inParam, &outParam);
				ret = cellPngDecExtSetParameter(mHandle, sHandle, &inParam, &outParam, &extInParam, &extoutParam);
				}
				else ret=-1;

			if(ret == CELL_OK)
				{
					dCtrlParam.outputBytesPerLine = _DW * 4;  
//					ret = cellPngDecDecodeData(mHandle, sHandle, data, &dCtrlParam, &dOutInfo);
					ret = cellPngDecExtDecodeData(mHandle, sHandle, data, &dCtrlParam, &dOutInfo, &pngDisp, &pngDispInfo);



					if((ret == CELL_OK) && (dOutInfo.status == CELL_PNGDEC_DEC_STATUS_FINISH))
						{
						png_w= outParam.outputWidth;
						png_h= outParam.outputHeight;
						ok=0;
						}
				}

			if(ret_file==0)	ret = cellPngDecClose(mHandle, sHandle);
			
			ret = cellPngDecDestroy(mHandle);
			
			}
	
	//InParam.spuThreadEnable   = CELL_PNGDEC_SPU_THREAD_DISABLE;

//	use_png_alpha=0;	
return ok;
}
*/

int load_jpg_texture_th(u8 *data, char *name, uint16_t _DW)
{
	int ret, ok=-1;
	jpg_w=0; jpg_h=0;

    CellJpgDecMainHandle     mHandle;
    CellJpgDecSubHandle      sHandle;

    CellJpgDecInParam        inParam;
    CellJpgDecOutParam       outParam;

    CellJpgDecSrc            src;
    CellJpgDecOpnInfo        opnInfo;
    CellJpgDecInfo           info;

    CellJpgDecDataOutInfo    dOutInfo;
    CellJpgDecDataCtrlParam  dCtrlParam;

    CellJpgDecThreadInParam  InParam;
    CellJpgDecThreadOutParam OutParam;

	CtrlMallocArg               MallocArg;
	CtrlFreeArg                 FreeArg;

    float                    downScale;
    bool                     unsupportFlag;

    MallocArg.mallocCallCounts  = 0;
    FreeArg.freeCallCounts      = 0;

//	InParam.spuThreadEnable   = CELL_JPGDEC_SPU_THREAD_DISABLE;
	InParam.spuThreadEnable   = CELL_JPGDEC_SPU_THREAD_ENABLE;
	InParam.ppuThreadPriority = 50;
	InParam.spuThreadPriority = 17;
	InParam.cbCtrlMallocFunc  = png_malloc;
	InParam.cbCtrlMallocArg   = &MallocArg;
	InParam.cbCtrlFreeFunc    = png_free;
	InParam.cbCtrlFreeArg     = &FreeArg;

    ret = cellJpgDecCreate(&mHandle, &InParam, &OutParam);

    if(ret == CELL_OK){

            src.srcSelect  = CELL_JPGDEC_FILE;
            src.fileName   = name;
            src.fileOffset = 0;
            src.fileSize   = 0;
            src.streamPtr  = NULL;
            src.streamSize = 0;

            src.spuThreadEnable = CELL_JPGDEC_SPU_THREAD_ENABLE;

			unsupportFlag = false;
            ret = cellJpgDecOpen(mHandle, &sHandle, &src, &opnInfo);
            if(ret == CELL_OK){    

                ret = cellJpgDecReadHeader(mHandle, sHandle, &info);
				if(info.jpegColorSpace == CELL_JPG_UNKNOWN){
					unsupportFlag = true;
				}

				if(ret !=CELL_OK || info.imageHeight==0)
				{
					src.spuThreadEnable = CELL_JPGDEC_SPU_THREAD_DISABLE;
					unsupportFlag = false;
		            ret = cellJpgDecClose(mHandle, sHandle);
					ret = cellJpgDecOpen(mHandle, &sHandle, &src, &opnInfo);
					ret = cellJpgDecReadHeader(mHandle, sHandle, &info);
					if(info.jpegColorSpace == CELL_JPG_UNKNOWN){
						unsupportFlag = true;
					}
				}


			} //decoder open

			if(ret == CELL_OK){
				if(scale_icon_h) 
				{
//					if(info.imageHeight>info.imageWidth)
					if( ((float)info.imageHeight / (float)XMB_THUMB_HEIGHT) > ((float)info.imageWidth / (float) XMB_THUMB_WIDTH))
						downScale=(float)info.imageHeight / (float)(XMB_THUMB_HEIGHT);
					else
						downScale=(float)info.imageWidth / (float) (XMB_THUMB_WIDTH);
				}
				else
				{

					if(info.imageWidth>1920 || info.imageHeight>1080){
						if( ((float)info.imageWidth / 1920) > ((float)info.imageHeight / 1080 ) ){
							downScale = (float)info.imageWidth / 1920;
						}else{
							downScale = (float)info.imageHeight / 1080; 
						}
					}
					else
						downScale=1.f;

					if(strstr(name, "/HDAVCTN/BDMT_O1.jpg")!=NULL || strstr(name, "/BDMV/META/DL/HDAVCTN_O1.jpg")!=NULL) downScale = (float) (info.imageWidth / 320);

				}

					if( downScale <= 1.f ){
						inParam.downScale = 1;
					}else if( downScale <= 2.f ){
						inParam.downScale = 2;
					}else if( downScale <= 4.f ){
						inParam.downScale = 4;
					}else{
						inParam.downScale = 8;
					}

					if(downScale>8.0f)
					{
						jpg_w=0; jpg_h=0;
						goto leave_jpg_th;

					}

           
                inParam.commandPtr       = NULL;
                inParam.method           = CELL_JPGDEC_FAST;
                inParam.outputMode       = CELL_JPGDEC_TOP_TO_BOTTOM;
                inParam.outputColorSpace = CELL_JPG_RGBA;
		//		if(scale_icon_h) 
	    //          inParam.outputColorAlpha = 0x80;
		//		else
		            inParam.outputColorAlpha = 0xfe;
                ret = cellJpgDecSetParameter(mHandle, sHandle, &inParam, &outParam);
            }

            if(ret == CELL_OK){
//				if( _DW<1920 ) 
					if(scale_icon_h && inParam.downScale) 
		                dCtrlParam.outputBytesPerLine = (int) ((info.imageWidth/inParam.downScale) * 4);
					else
		                dCtrlParam.outputBytesPerLine = _DW * 4;

//				else
//		            dCtrlParam.outputBytesPerLine = 1920 * 4;
//                memset(data, 0, sizeof(data));
                
                ret = cellJpgDecDecodeData(mHandle, sHandle, data, &dCtrlParam, &dOutInfo);
                
				if((ret == CELL_OK) && (dOutInfo.status == CELL_JPGDEC_DEC_STATUS_FINISH))
					{
					jpg_w= outParam.outputWidth;
					jpg_h= outParam.outputHeight;
					ok=0;
					}
            }

leave_jpg_th:
            ret = cellJpgDecClose(mHandle, sHandle);
		    ret = cellJpgDecDestroy(mHandle);
			} //decoder create

	scale_icon_h=0;
	return ret;
}

int load_jpg_texture(u8 *data, char *name, uint16_t _DW)
{
	scale_icon_h=0;
	while(is_decoding_jpg || is_decoding_png){ sys_timer_usleep(3336); cellSysutilCheckCallback();}
	is_decoding_jpg=1;
	int ret, ok=-1;
	png_w=0; png_h=0;
	is_decoding_jpg=1;
    CellJpgDecMainHandle     mHandle;
    CellJpgDecSubHandle      sHandle;

    CellJpgDecInParam        inParam;
    CellJpgDecOutParam       outParam;

    CellJpgDecSrc            src;
    CellJpgDecOpnInfo        opnInfo;
    CellJpgDecInfo           info;

    CellJpgDecDataOutInfo    dOutInfo;
    CellJpgDecDataCtrlParam  dCtrlParam;

    CellJpgDecThreadInParam  InParam;
    CellJpgDecThreadOutParam OutParam;

	CtrlMallocArg               MallocArg;
	CtrlFreeArg                 FreeArg;

    float                    downScale;
    bool                     unsupportFlag;

    MallocArg.mallocCallCounts  = 0;
    FreeArg.freeCallCounts      = 0;

//	InParam.spuThreadEnable   = CELL_JPGDEC_SPU_THREAD_DISABLE;
	InParam.spuThreadEnable   = CELL_JPGDEC_SPU_THREAD_ENABLE;
	InParam.ppuThreadPriority = 128;
	InParam.spuThreadPriority = 25;
	InParam.cbCtrlMallocFunc  = png_malloc;
	InParam.cbCtrlMallocArg   = &MallocArg;
	InParam.cbCtrlFreeFunc    = png_free;
	InParam.cbCtrlFreeArg     = &FreeArg;

    ret = cellJpgDecCreate(&mHandle, &InParam, &OutParam);

    if(ret == CELL_OK){

            src.srcSelect  = CELL_JPGDEC_FILE;
            src.fileName   = name;
            src.fileOffset = 0;
            src.fileSize   = 0;
            src.streamPtr  = NULL;
            src.streamSize = 0;

            src.spuThreadEnable = CELL_JPGDEC_SPU_THREAD_ENABLE;

			unsupportFlag = false;
            ret = cellJpgDecOpen(mHandle, &sHandle, &src, &opnInfo);
            if(ret == CELL_OK){    

                ret = cellJpgDecReadHeader(mHandle, sHandle, &info);
				if(info.jpegColorSpace == CELL_JPG_UNKNOWN){
					unsupportFlag = true;
				}

				if(ret !=CELL_OK || info.imageHeight==0)
				{
					src.spuThreadEnable = CELL_JPGDEC_SPU_THREAD_DISABLE;
					unsupportFlag = false;
		            ret = cellJpgDecClose(mHandle, sHandle);
					ret = cellJpgDecOpen(mHandle, &sHandle, &src, &opnInfo);
					ret = cellJpgDecReadHeader(mHandle, sHandle, &info);
					if(info.jpegColorSpace == CELL_JPG_UNKNOWN){
						unsupportFlag = true;
					}
				}


			} //decoder open

			if(ret == CELL_OK){
				if(scale_icon_h) 
				{
//					if(info.imageHeight>info.imageWidth)
					if( ((float)info.imageHeight / (float)XMB_THUMB_HEIGHT) > ((float)info.imageWidth / (float) XMB_THUMB_WIDTH))
						downScale=(float)info.imageHeight / (float)(XMB_THUMB_HEIGHT);
					else
						downScale=(float)info.imageWidth / (float) (XMB_THUMB_WIDTH);
				}
				else
				{

					if(info.imageWidth>1920 || info.imageHeight>1080){
						if( ((float)info.imageWidth / 1920) > ((float)info.imageHeight / 1080 ) ){
							downScale = (float)info.imageWidth / 1920;
						}else{
							downScale = (float)info.imageHeight / 1080; 
						}
					}
					else
						downScale=1.f;

					if(strstr(name, "/HDAVCTN/BDMT_O1.jpg")!=NULL || strstr(name, "/BDMV/META/DL/HDAVCTN_O1.jpg")!=NULL) downScale = (float) (info.imageWidth / 320);

				}

					if( downScale <= 1.f ){
						inParam.downScale = 1;
					}else if( downScale <= 2.f ){
						inParam.downScale = 2;
					}else if( downScale <= 4.f ){
						inParam.downScale = 4;
					}else{
						inParam.downScale = 8;
					}

					if(downScale>8.0f)
					{
						png_w=0;
						png_h=0;
						goto leave_jpg;

					}

           
                inParam.commandPtr       = NULL;
                inParam.method           = CELL_JPGDEC_FAST;
                inParam.outputMode       = CELL_JPGDEC_TOP_TO_BOTTOM;
                inParam.outputColorSpace = CELL_JPG_RGBA;
		//		if(scale_icon_h) 
	    //          inParam.outputColorAlpha = 0x80;
		//		else
		            inParam.outputColorAlpha = 0xfe;
                ret = cellJpgDecSetParameter(mHandle, sHandle, &inParam, &outParam);
            }

            if(ret == CELL_OK){
//				if( _DW<1920 ) 
					if(scale_icon_h && inParam.downScale) 
		                dCtrlParam.outputBytesPerLine = (int) ((info.imageWidth/inParam.downScale) * 4);
					else
		                dCtrlParam.outputBytesPerLine = _DW * 4;

//				else
//		            dCtrlParam.outputBytesPerLine = 1920 * 4;
//                memset(data, 0, sizeof(data));
                
                ret = cellJpgDecDecodeData(mHandle, sHandle, data, &dCtrlParam, &dOutInfo);
                
				if((ret == CELL_OK) && (dOutInfo.status == CELL_JPGDEC_DEC_STATUS_FINISH))
					{
					png_w= outParam.outputWidth;
					png_h= outParam.outputHeight;
					ok=0;
					}
            }

leave_jpg:
            ret = cellJpgDecClose(mHandle, sHandle);
		    ret = cellJpgDecDestroy(mHandle);
			} //decoder create

	scale_icon_h=0;
	is_decoding_jpg=0;
	return ret;
}

int load_png_texture_th(u8 *data, char *name)//, uint16_t _DW)
{

	int  ret_file, ret, ok=-1;
	
	CellPngDecMainHandle        mHandle;
	CellPngDecSubHandle         sHandle;

	CellPngDecThreadInParam 	InParam;
	CellPngDecThreadOutParam 	OutParam;

	CellPngDecSrc 		        src; 
	CellPngDecOpnInfo 	        opnInfo;
	CellPngDecInfo 		        info;

	CellPngDecDataOutInfo 	    dOutInfo;
	CellPngDecDataCtrlParam     dCtrlParam;
	CellPngDecInParam 	        inParam;
	CellPngDecOutParam 	        outParam;

	CtrlMallocArg               MallocArg;
	CtrlFreeArg                 FreeArg;

	int ret_png=-1;

//	InParam.spuThreadEnable   = CELL_PNGDEC_SPU_THREAD_DISABLE;
	InParam.spuThreadEnable   = CELL_PNGDEC_SPU_THREAD_ENABLE;
	InParam.ppuThreadPriority = 50;
	InParam.spuThreadPriority = 17;
	InParam.cbCtrlMallocFunc  = png_malloc;
	InParam.cbCtrlMallocArg   = &MallocArg;
	InParam.cbCtrlFreeFunc    = png_free;
	InParam.cbCtrlFreeArg     = &FreeArg;
	

	ret_png= ret= cellPngDecCreate(&mHandle, &InParam, &OutParam);
	
//	memset(data, 0x00, sizeof(data)); //(DISPLAY_WIDTH * DISPLAY_HEIGHT * 4)

	png_w_th= png_h_th= 0;

	if(ret_png == CELL_OK)
		{
		
			memset(&src, 0, sizeof(CellPngDecSrc));
			src.srcSelect     = CELL_PNGDEC_FILE;
			src.fileName      = name;

//			src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_DISABLE;
			src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_ENABLE;
			
			ret_file=ret = cellPngDecOpen(mHandle, &sHandle, &src, &opnInfo);
			
			if(ret == CELL_OK)
			{
				ret = cellPngDecReadHeader(mHandle, sHandle, &info);

				if(ret !=CELL_OK || info.imageHeight==0)
				{
					src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_DISABLE;
					cellPngDecClose(mHandle, sHandle);
					ret_file=ret = cellPngDecOpen(mHandle, &sHandle, &src, &opnInfo);
					ret = cellPngDecReadHeader(mHandle, sHandle, &info);
				}

			}

			if(ret == CELL_OK)// && (_DW * info.imageHeight <= 2073600))
				{	
				inParam.commandPtr        = NULL;
				inParam.outputMode        = CELL_PNGDEC_TOP_TO_BOTTOM;
				inParam.outputColorSpace  = CELL_PNGDEC_RGBA;
				inParam.outputBitDepth    = 8;
				inParam.outputPackFlag    = CELL_PNGDEC_1BYTE_PER_1PIXEL;

				if((info.colorSpace == CELL_PNGDEC_GRAYSCALE_ALPHA) || (info.colorSpace == CELL_PNGDEC_RGBA) || (info.chunkInformation & 0x10))
					inParam.outputAlphaSelect = CELL_PNGDEC_STREAM_ALPHA;
				else
					inParam.outputAlphaSelect = CELL_PNGDEC_FIX_ALPHA;

//				if(use_png_alpha)
//					inParam.outputAlphaSelect = CELL_PNGDEC_STREAM_ALPHA;
//				else
					inParam.outputColorAlpha  = 0xff;

//				inParam.outputColorAlpha  = 0x00;


				ret = cellPngDecSetParameter(mHandle, sHandle, &inParam, &outParam);
				}
				else ret=-1;

			if(ret == CELL_OK)
				{
					dCtrlParam.outputBytesPerLine =  info.imageWidth * 4;//_DW * 4;  
					ret = cellPngDecDecodeData(mHandle, sHandle, data, &dCtrlParam, &dOutInfo);

//					sys_timer_usleep(500);

					if((ret == CELL_OK) && (dOutInfo.status == CELL_PNGDEC_DEC_STATUS_FINISH))
						{
						png_w_th= outParam.outputWidth;
						png_h_th= outParam.outputHeight;
						ok=0;
						}
				}

			if(ret_file==0)	ret = cellPngDecClose(mHandle, sHandle);
			
			ret = cellPngDecDestroy(mHandle);
			
			}
	
	//InParam.spuThreadEnable   = CELL_PNGDEC_SPU_THREAD_DISABLE;

//	use_png_alpha=0;	
return ok;
}


int load_png_texture(u8 *data, char *name, uint16_t _DW)
{
	while(is_decoding_jpg || is_decoding_png){ sys_timer_usleep(3336); cellSysutilCheckCallback();}
	is_decoding_png=1;
	int  ret_file, ret, ok=-1;
	
	CellPngDecMainHandle        mHandle;
	CellPngDecSubHandle         sHandle;

	CellPngDecThreadInParam 	InParam;
	CellPngDecThreadOutParam 	OutParam;

	CellPngDecSrc 		        src; 
	CellPngDecOpnInfo 	        opnInfo;
	CellPngDecInfo 		        info;

	CellPngDecDataOutInfo 	    dOutInfo;
	CellPngDecDataCtrlParam     dCtrlParam;
	CellPngDecInParam 	        inParam;
	CellPngDecOutParam 	        outParam;

	CtrlMallocArg               MallocArg;
	CtrlFreeArg                 FreeArg;

	int ret_png=-1;

//	InParam.spuThreadEnable   = CELL_PNGDEC_SPU_THREAD_DISABLE;
	InParam.spuThreadEnable   = CELL_PNGDEC_SPU_THREAD_ENABLE;
	InParam.ppuThreadPriority = 50;
	InParam.spuThreadPriority = 17;
	InParam.cbCtrlMallocFunc  = png_malloc;
	InParam.cbCtrlMallocArg   = &MallocArg;
	InParam.cbCtrlFreeFunc    = png_free;
	InParam.cbCtrlFreeArg     = &FreeArg;
	

	ret_png= ret= cellPngDecCreate(&mHandle, &InParam, &OutParam);
	
//	memset(data, 0x00, sizeof(data)); //(DISPLAY_WIDTH * DISPLAY_HEIGHT * 4)

	png_w= png_h= 0;

	if(ret_png == CELL_OK)
		{
		
			memset(&src, 0, sizeof(CellPngDecSrc));
			src.srcSelect     = CELL_PNGDEC_FILE;
			src.fileName      = name;

//			src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_DISABLE;
			src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_ENABLE;
			
			ret_file=ret = cellPngDecOpen(mHandle, &sHandle, &src, &opnInfo);
			
			if(ret == CELL_OK)
			{
				ret = cellPngDecReadHeader(mHandle, sHandle, &info);

				if(ret !=CELL_OK || info.imageHeight==0)
				{
					src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_DISABLE;
					cellPngDecClose(mHandle, sHandle);
					ret_file=ret = cellPngDecOpen(mHandle, &sHandle, &src, &opnInfo);
					ret = cellPngDecReadHeader(mHandle, sHandle, &info);
				}

			}

			if(ret == CELL_OK && (_DW * info.imageHeight <= 2073600))
				{	
				inParam.commandPtr        = NULL;
				inParam.outputMode        = CELL_PNGDEC_TOP_TO_BOTTOM;
				inParam.outputColorSpace  = CELL_PNGDEC_RGBA;
				inParam.outputBitDepth    = 8;
				inParam.outputPackFlag    = CELL_PNGDEC_1BYTE_PER_1PIXEL;

				if((info.colorSpace == CELL_PNGDEC_GRAYSCALE_ALPHA) || (info.colorSpace == CELL_PNGDEC_RGBA) || (info.chunkInformation & 0x10))
					inParam.outputAlphaSelect = CELL_PNGDEC_STREAM_ALPHA;
				else
					inParam.outputAlphaSelect = CELL_PNGDEC_FIX_ALPHA;

//				if(use_png_alpha)
//					inParam.outputAlphaSelect = CELL_PNGDEC_STREAM_ALPHA;
//				else
					inParam.outputColorAlpha  = 0xff;

//				inParam.outputColorAlpha  = 0x00;


				ret = cellPngDecSetParameter(mHandle, sHandle, &inParam, &outParam);
				}
				else ret=-1;

			if(ret == CELL_OK)
				{
					dCtrlParam.outputBytesPerLine = _DW * 4;  
					ret = cellPngDecDecodeData(mHandle, sHandle, data, &dCtrlParam, &dOutInfo);

//					sys_timer_usleep(500);

					if((ret == CELL_OK) && (dOutInfo.status == CELL_PNGDEC_DEC_STATUS_FINISH))
						{
						png_w= outParam.outputWidth;
						png_h= outParam.outputHeight;
						ok=0;
						}
				}

			if(ret_file==0)	ret = cellPngDecClose(mHandle, sHandle);
			
			ret = cellPngDecDestroy(mHandle);
			
			}
	
	//InParam.spuThreadEnable   = CELL_PNGDEC_SPU_THREAD_DISABLE;

//	use_png_alpha=0;	
	is_decoding_png=0;
return ok;
}

int load_raw_texture(u8 *data, char *name, uint16_t _DW)
{
		FILE *fpA;

		uint32_t _DWO=80, _DHO=45;
		if(strstr(name, "_960.RAW")!=NULL) { _DWO=960; _DHO=540; }
		if(strstr(name, "_640.RAW")!=NULL) { _DWO=640; _DHO=360; }
		if(strstr(name, "_480.RAW")!=NULL) { _DWO=480; _DHO=270; }
		if(strstr(name, "_320.RAW")!=NULL) { _DWO=320; _DHO=180; }
		if(strstr(name, "_240.RAW")!=NULL) { _DWO=240; _DHO=135; }
		if(strstr(name, "_160.RAW")!=NULL) { _DWO=160; _DHO= 90; }
		if(strstr(name,  "_80.RAW")!=NULL) { _DWO= 80; _DHO= 45; }

		fpA = fopen ( name, "rb" );
		if (fpA != NULL)
		{

		fseek(fpA, 0, SEEK_SET);
		if(_DW!=_DWO)
		{
			fread(buf, (_DWO * _DHO * 4), 1, fpA);
			mip_texture( data, (uint8_t *)buf, _DWO, _DHO, (_DW/_DWO)); //scale to 1920x1080
		}
		else
		{
			fread((u8*)data, (_DWO * _DHO * 4), 1, fpA);
//			fread(buf, (_DWO * _DHO * 4), 1, fpA);
//			memcpy(data, buf, (_DWO * _DHO * 4));
		}

		fclose(fpA);

//		int blur=(_DW/_DWO)-1;
//		if(blur>3) blur=3;
//		blur_texture( data, _DW, _DHO*(_DW/_DWO), 0, 0, _DW, _DHO*(_DW/_DWO), 0, 0, 1, blur);

		return 1;
		}
		return 0;

}

int load_texture(u8 *data, char *name, uint16_t dw)
{

	if(strstr(name, ".jpg")!=NULL || strstr(name, ".JPG")!=NULL || strstr(name, ".jpeg")!=NULL || strstr(name, ".JPEG")!=NULL) 
		load_jpg_texture( data, name, dw);
	else if(strstr(name, ".png")!=NULL || strstr(name, ".PNG")!=NULL) 
	{
//		if(data==text_bmp && dw==1920)
//			load_png_partial( data, name, dw, 18, 0);
//		else
			load_png_texture( data, name, dw);
	}
	else if(strstr(name, ".RAW")!=NULL) load_raw_texture( data, name, dw);
	return 0;

}
/****************************************************/
/* syscalls                                         */
/****************************************************/

static void poke_sc36_path( const char *path)
{

	if(sc36_path_patch==0 || payload!=0 || c_firmware!=3.55f || strstr(path, "/dev_bdvd")!=NULL) return;
	char r_path[64];
	u64 p_len;
	u64 val0=0x0000000000000000ULL;
	u64 base=0x80000000002D84DEULL;
	u64 val=0x0000000000000000ULL;
	strncpy(r_path,  path, 18); r_path[19]=0;
    u8 * p = (u8 *) r_path;
	p_len=strlen(r_path); if(p_len>18) p_len=18;

    int n=0;
    for(n = 0; n < 24; n += 8) {
		 if(n==16 && p_len<=16) break;
         memcpy(&val, &p[n], 8);
         pokeq(base + (u64) n, val);
		 if(n>15) pokeq(0x80000000002D84F0ULL, 0x7FA3EB783BE00001ULL );
         __asm__("sync");
         val0=peekq(0x8000000000000000ULL);
	}

	u64 val1 = 0x38A000004BD761D1ULL;
	u64 val2 = 0x389D00004BD76155ULL;

    val2 = (val2) | ( (p_len) << 32);
	if(strstr(path, "/app_home")!=NULL) p_len=2;
    val1 = (val1) | ( (p_len) << 32);

	pokeq(0x80000000002D8504ULL, val1 );
	pokeq(0x80000000002D852CULL, val2 );

//	pokeq(0x80000000003F662DULL, 0x6170705F686F6D65ULL ); // /app_home -> /app_home
//	pokeq(0x80000000003F662DULL, 0x6465765F62647664ULL ); // /app_home -> /dev_bdvd
//	pokeq(0x80000000003F672DULL, 0x6465765F62647664ULL ); // /host_root -> /dev_bdvd
//	pokeq(0x80000000003F6735ULL, 0x0000000000000000ULL ); // /host_root -> /dev_bdvd

    __asm__("sync");
    val0=peekq(0x8000000000000000ULL);
}

void pokeq( uint64_t addr, uint64_t val)
{ 
	if(c_firmware!=3.55f && c_firmware!=3.41f && c_firmware!=3.15f) return;
	system_call_2(7, addr, val); 
} 

uint64_t peekq(uint64_t addr)
{
	if(c_firmware!=3.55f && c_firmware!=3.41f && c_firmware!=3.15f) return 0;
	system_call_1(6, addr);
	return_to_user_prog(uint64_t);
}

void disable_sc36()
{
	return;
		if( (peekq(0x80000000002D8488ULL) == 0x3BE000017BFFF806ULL) && payloadT[0]==0x44 ) // syscall36 enabled
		   pokeq(0x80000000002D8488ULL,    0x480000447BFFF806ULL ); // syscall36 disable!

}

void enable_sc36()
{
	return;
		if( (peekq(0x80000000002D8488ULL) == 0x480000447BFFF806ULL) && payloadT[0]==0x44) // syscall36 disabled
		   pokeq(0x80000000002D8488ULL,    0x3BE000017BFFF806ULL ); // syscall36 enable!
}

static uint32_t syscall35(const char *srcpath, const char *dstpath)
{
	if(payload==-1) return 0;
	system_call_2(35, (uint32_t) srcpath, (uint32_t) dstpath);
	return_to_user_prog(uint32_t);
}

static void syscall_mount(const char *path,  int mountbdvd)
{
	if(mountbdvd==0 || payload==-1) return;

	if(payload!=2)
	{
		system_call_1(36, (uint32_t) path);
	}

	if(payload==2)
	{
		(void) syscall35("/dev_bdvd", path);
		(void) syscall35("/app_home", path);
	}

}

static void syscall_mount2(char *mountpoint, const char *path)
{
				if(payload==-1) return;

				if(payload==0) { //PSGroove
					poke_sc36_path( (char *) mountpoint);
					system_call_1(36, (uint32_t) path);
				}

				if(payload==1) { //Hermes
				syscall_mount( (char*)path, mount_bdvd);
				(void)sys8_path_table(0ULL);
				dest_table_addr= 0x80000000007FF000ULL-((sizeof(path_open_table)+15) & ~15);
				open_table.entries[0].compare_addr= ((uint64_t) &open_table.arena[0]) - ((uint64_t) &open_table) + dest_table_addr;
				open_table.entries[0].replace_addr= ((uint64_t) &open_table.arena[0x200])- ((uint64_t) &open_table) + dest_table_addr;
				open_table.entries[1].compare_addr= 0ULL; // the last entry always 0

				strncpy(&open_table.arena[0], mountpoint, 0x200);    // compare 1
				strncpy(&open_table.arena[0x200], path, 0x200);     // replace 1
				open_table.entries[0].compare_len= strlen(&open_table.arena[0]);		// 1
				open_table.entries[0].replace_len= strlen(&open_table.arena[0x200]);
				sys8_memcpy(dest_table_addr, (uint64_t) &open_table, sizeof(path_open_table2));
				(void)sys8_path_table( dest_table_addr);
				}

				if(payload==2) { //PL3
//					(void)syscall35((char *)mountpoint, NULL);
					(void)syscall35((char *)mountpoint, (char *)path);
				}

}


static void mount_with_cache(const char *path, int _joined, u32 flags, const char *title_id)
{
				if(payload!=1) return; // leave if payload is not Hermes
				if(!(flags & IS_BDMIRROR))
					syscall_mount( (char*)path, mount_bdvd);

				(void) title_id;
				char s1[512];
				char s2[512];
				//char s1a[512];
				//char s2a[512];
				char s_tmp[512];

				char cached_file[512];

				char ext_gd_path[512];
				char hdd_gd_path[512];
				u8 no_gd=0;

				if(strstr(path, "/dev_usb")!=NULL || (flags & IS_BDMIRROR))
				{
					if(flags & IS_BDMIRROR)
					{sprintf(s_tmp, "%s", "/dev_bdvd");s_tmp[9]=0;}
					else
					{strncpy(s_tmp, path, 11); s_tmp[11]=0;}
					if((flags & IS_EXTGD))
					{
						sprintf(ext_gd_path, "%s/GAMEI", s_tmp);
						mkdir(ext_gd_path, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(ext_gd_path, 0777);
					}
					//sprintf(ext_gd_path, "%s/GAMEI/%s", s_tmp, title_id);
				}
				else
				{
					for(int u=0;u<200;u++)
					{
						sprintf(s_tmp, "/dev_usb%03i", u);
						if(stat(s_tmp, &s3)>=0) break;
					}
					if(stat(s_tmp, &s3)>=0 && (flags & IS_EXTGD))
					{
						sprintf(ext_gd_path, "%s/GAMEI", s_tmp);
						mkdir(ext_gd_path, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(ext_gd_path, 0777);
						fix_perm_recursive(ext_gd_path);
						//sprintf(ext_gd_path, "%s/GAMEI/%s", s_tmp, title_id);
					}
					else no_gd=1;
				}


				//sprintf(s1a, "%s", "/dev_bdvd");s1a[9]=0;
				//sprintf(s2a, "%s", "/app_home");s2a[9]=0;
				u8 entries=0;
				u32 m_step=0x200;
				
				(void)sys8_path_table(0ULL);
				dest_table_addr= 0x80000000007FF000ULL-((sizeof(path_open_table)+15) & ~15);
				for(int n=0; n<_joined; n++)
				{
					sprintf(s1, "/dev_bdvd%s", file_to_join[n].split_file);
					sprintf(s2, "/app_home%s", file_to_join[n].split_file);
					sprintf(cached_file, "%s", file_to_join[n].cached_file);

					//sprintf(filename, "#%i [%s]\n[%s]\n[%s]", n+1, s1, s2, cached_file);
					//dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();

					strncpy(&open_table.arena[entries*m_step*2], s1, m_step);
					strncpy(&open_table.arena[entries*m_step*2+m_step], cached_file, m_step);
					open_table.entries[entries].compare_addr= ((uint64_t) &open_table.arena[entries*m_step*2]) - ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].compare_len= strlen(&open_table.arena[entries*m_step*2]);
					open_table.entries[entries].replace_addr= ((uint64_t) &open_table.arena[entries*m_step*2+m_step])- ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].replace_len= strlen(&open_table.arena[entries*m_step*2+m_step]);
					entries++;

					strncpy(&open_table.arena[entries*m_step*2], s2, m_step);
					strncpy(&open_table.arena[entries*m_step*2+m_step], cached_file, m_step);
					open_table.entries[entries].compare_addr= ((uint64_t) &open_table.arena[entries*m_step*2]) - ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].compare_len= strlen(&open_table.arena[entries*m_step*2]);
					open_table.entries[entries].replace_addr= ((uint64_t) &open_table.arena[entries*m_step*2+m_step])- ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].replace_len= strlen(&open_table.arena[entries*m_step*2+m_step]);
					entries++;
				}

				if( (flags & IS_EXTGD) && no_gd==0 )
				{
					sprintf(hdd_gd_path, "%s", "/dev_hdd0/game");///%s", title_id);
					strncpy(&open_table.arena[entries*m_step*2], hdd_gd_path	, m_step);
					strncpy(&open_table.arena[entries*m_step*2+m_step], ext_gd_path, m_step);
					open_table.entries[entries].compare_addr= ((uint64_t) &open_table.arena[entries*m_step*2]) - ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].compare_len= strlen(&open_table.arena[entries*m_step*2]);
					open_table.entries[entries].replace_addr= ((uint64_t) &open_table.arena[entries*m_step*2+m_step])- ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].replace_len= strlen(&open_table.arena[entries*m_step*2+m_step]);
					entries++;
				}

				/*if(!(flags & IS_BDMIRROR))
				{
					strncpy(&open_table.arena[entries*m_step*2], s1a, m_step);
					strncpy(&open_table.arena[entries*m_step*2+m_step], path, m_step);
					open_table.entries[entries].compare_addr= ((uint64_t) &open_table.arena[entries*m_step*2]) - ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].compare_len= strlen(&open_table.arena[entries*m_step*2]);
					open_table.entries[entries].replace_addr= ((uint64_t) &open_table.arena[entries*m_step*2+m_step])- ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].replace_len= strlen(&open_table.arena[entries*m_step*2+m_step]);
					entries++;

					strncpy(&open_table.arena[entries*m_step*2], s2a, m_step);
					strncpy(&open_table.arena[entries*m_step*2+m_step], path, m_step);
					open_table.entries[entries].compare_addr= ((uint64_t) &open_table.arena[entries*m_step*2]) - ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].compare_len= strlen(&open_table.arena[entries*m_step*2]);
					open_table.entries[entries].replace_addr= ((uint64_t) &open_table.arena[entries*m_step*2+m_step])- ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].replace_len= strlen(&open_table.arena[entries*m_step*2+m_step]);
					entries++;
				} */
					open_table.entries[entries].compare_addr= 0ULL;

				sys8_memcpy(dest_table_addr, (uint64_t) &open_table, sizeof(path_open_table));
				(void)sys8_path_table( dest_table_addr);
}


static void mount_with_ext_data(const char *path, u32 flags)
{
				if(!(flags & IS_BDMIRROR))
					syscall_mount( (char*)path, mount_bdvd);

				if(payload!=1 || !(flags & IS_EXTGD)) return; //leave if not Hermes payload or not flagged for External Game Data

				//char s1a[512];
				//char s2a[512];
				char s_tmp[512];
				char ext_gd_path[512];
				char hdd_gd_path[512];
				u8 no_gd=0;

				if(strstr(path, "/dev_usb")!=NULL || (flags & IS_BDMIRROR))
				{
					if(flags & IS_BDMIRROR)
					{sprintf(s_tmp, "%s", "/dev_bdvd");s_tmp[9]=0;}
					else
					{strncpy(s_tmp, path, 11); s_tmp[11]=0;}

					if((flags & IS_EXTGD))
					{
						sprintf(ext_gd_path, "%s/GAMEI", s_tmp);
						mkdir(ext_gd_path, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(ext_gd_path, 0777);
						//sprintf(ext_gd_path, "%s/GAMEI/%s", s_tmp, title_id);
					}
				}
				else
				{
					for(int u=0;u<200;u++)
					{
						sprintf(s_tmp, "/dev_usb%03i", u);
						if(stat(s_tmp, &s3)>=0) break;
					}
					if(stat(s_tmp, &s3)>=0 && (flags & IS_EXTGD))
					{
						sprintf(ext_gd_path, "%s/GAMEI", s_tmp);
						mkdir(ext_gd_path, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(ext_gd_path, 0777);
						//sprintf(ext_gd_path, "%s/GAMEI/%s", s_tmp, title_id);
					}
					else no_gd=1;
				}


				//sprintf(s1a, "%s", "/dev_bdvd");s1a[9]=0;
				//sprintf(s2a, "%s", "/app_home");s2a[9]=0;
				u8 entries=0;
				u32 m_step=0x200;
				
				(void)sys8_path_table(0ULL);
				dest_table_addr= 0x80000000007FF000ULL-((sizeof(path_open_table)+15) & ~15);

				if( (flags & IS_EXTGD) && no_gd==0 )
				{
					sprintf(hdd_gd_path, "%s", "/dev_hdd0/game");///%s", title_id);
					strncpy(&open_table.arena[entries*m_step*2], hdd_gd_path	, m_step);
					strncpy(&open_table.arena[entries*m_step*2+m_step], ext_gd_path, m_step);
					open_table.entries[entries].compare_addr= ((uint64_t) &open_table.arena[entries*m_step*2]) - ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].compare_len= strlen(&open_table.arena[entries*m_step*2]);
					open_table.entries[entries].replace_addr= ((uint64_t) &open_table.arena[entries*m_step*2+m_step])- ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].replace_len= strlen(&open_table.arena[entries*m_step*2+m_step]);
					entries++;
				}

				/*if(!(flags & IS_BDMIRROR))
				{
					strncpy(&open_table.arena[entries*m_step*2], s1a, m_step);
					strncpy(&open_table.arena[entries*m_step*2+m_step], path, m_step);
					open_table.entries[entries].compare_addr= ((uint64_t) &open_table.arena[entries*m_step*2]) - ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].compare_len= strlen(&open_table.arena[entries*m_step*2]);
					open_table.entries[entries].replace_addr= ((uint64_t) &open_table.arena[entries*m_step*2+m_step])- ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].replace_len= strlen(&open_table.arena[entries*m_step*2+m_step]);
					entries++;

					strncpy(&open_table.arena[entries*m_step*2], s2a, m_step);
					strncpy(&open_table.arena[entries*m_step*2+m_step], path, m_step);
					open_table.entries[entries].compare_addr= ((uint64_t) &open_table.arena[entries*m_step*2]) - ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].compare_len= strlen(&open_table.arena[entries*m_step*2]);
					open_table.entries[entries].replace_addr= ((uint64_t) &open_table.arena[entries*m_step*2+m_step])- ((uint64_t) &open_table) + dest_table_addr;
					open_table.entries[entries].replace_len= strlen(&open_table.arena[entries*m_step*2+m_step]);
					entries++;
				}*/
					open_table.entries[entries].compare_addr= 0ULL;

				sys8_memcpy(dest_table_addr, (uint64_t) &open_table, sizeof(path_open_table));
				(void)sys8_path_table( dest_table_addr);
}


static void reset_mount_points()
{
				if(payload==-1) return;

				//syscall_838("/dev_bdvd");
				//syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_UDF", "/dev_bdvd", 0, 1, 0, 0, 0);

				if(payload!=2) { //Hermes
					poke_sc36_path( (char *) "/app_home");
					system_call_1(36, (uint32_t) "/dev_bdvd");
				}

				if(payload==1) { //Hermes
					(void)sys8_path_table(0ULL);
					system_call_1(36, (uint32_t) "/dev_bdvd");
				}

				if(payload==2) { //PL3
					(void)syscall35((char *)"/dev_bdvd", NULL);//(char *)"/dev_bdvd"
					(void)syscall35((char *)"/app_home", (char *)"/dev_usb000");
				}

}

static void mp3_callback( int streamNumber, void *userData,	int callbackType,	void *readBuffer,	int readSize)
{
		
		(void) streamNumber; (void)userData; (void)readBuffer; (void)readSize;
		if(callbackType!=0)
		{
			mp3_status=callbackType;
//			if(mp3_status==CELL_MS_CALLBACK_MOREDATA)//more data
			{
				/*char string1[32];
				sprintf(string1, "%i", callbackType);
				dialog_ret=0;
				cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL );
				wait_dialog();*/
			}
			if(max_mp3!=0){
				current_mp3++;
				if(current_mp3>max_mp3) current_mp3=1;
				main_mp3((char*) mp3_playlist[current_mp3].path);
			}
		}
}


static void unknown_mimetype_callback(const char* mimetype, const char* url, void* usrdata)
{
//	sprintf(www_info, "%s(): mimetype:%s url:%s userdata:0x%p\n", __FUNCTION__, mimetype, url, usrdata);
	(void) mimetype;
	(void) usrdata;
	char local_file_d[512], tempfileD[512]; tempfileD[0]=0;
	sprintf(local_file_d, "%s/DOWNLOADS", app_usrdir); 
	if(stat(download_dir, &s3)>=0 || strstr(download_dir, "/dev_")!=NULL) sprintf(local_file_d, "%s", download_dir);
	mkdir(local_file_d, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(local_file_d, 0777);
	sprintf(tempfileD, "%s", url);
	char *pathpos=strrchr(tempfileD, '/');
	if(stat(download_dir, &s3)>=0) 
		sprintf(local_file_d, "%s/%s", download_dir, (pathpos+1));
	else
		sprintf(local_file_d, "%s/DOWNLOADS/%s", app_usrdir, (pathpos+1));
	sprintf(www_info, "File download requested : (%s)", local_file_d);
	download_file( url, (char *) local_file_d, 3);
//	sprintf(local_file_d, "%s/DOWNLOADS/testfile.bin", app_usrdir);
//	download_file( url, (char *) local_file_d, 3);
}

DECL_WEBBROWSER_SYSTEM_CALLBACK(system_callback, cb_type, userdata)
{
	(void)userdata;
	switch (cb_type) {
	case CELL_SYSUTIL_WEBBROWSER_UNLOADING_FINISHED:
		exit_flag_ = true;
		www_running = 0;
		dialog_ret=3;
		cellWebBrowserShutdown();
	
		//sys_memory_container_destroy( memory_container_web );
		//sys_memory_container_destroy( memory_container );
		//MEMORY_CONTAINER_SIZE_ACTIVE=MEMORY_CONTAINER_SIZE;
		//sys_memory_container_create( &memory_container, MEMORY_CONTAINER_SIZE_ACTIVE);


	//case CELL_SYSUTIL_WEBBROWSER_RELEASED:
	case CELL_SYSUTIL_WEBBROWSER_SHUTDOWN_FINISHED:
		exit_flag_ = true;
		www_running = 0;
		dialog_ret=3;
		sys_memory_container_destroy( memory_container_web );
//		sys_memory_container_create( &memory_container, MEMORY_CONTAINER_SIZE);
		break;

	case CELL_SYSUTIL_REQUEST_EXITGAME: 
		no_callback=0;
		unload_modules(); sys_process_exit(1); break;


	default:
		break;
	}
}

static void sysutil_callback( uint64_t status, uint64_t param, void * userdata )
{
	(void)param;
	(void)userdata;
	int ret=0;
	switch(status)
	{

	case CELL_SYSUTIL_REQUEST_EXITGAME: 
		no_callback=0;
		unload_modules(); sys_process_exit(1); break;

	case CELL_SYSUTIL_OSKDIALOG_LOADED:
		break;

	case CELL_SYSUTIL_OSKDIALOG_INPUT_CANCELED:
		osk_dialog=-1;
		enteredCounter=0;
		osk_open=0;
		ret = cellOskDialogAbort();
		ret = cellOskDialogUnloadAsync(&OutputInfo);
		break;

	case CELL_SYSUTIL_OSKDIALOG_FINISHED:
		if(osk_dialog!=-1) osk_dialog=1;
		ret = cellOskDialogUnloadAsync(&OutputInfo);
		break;

	case CELL_SYSUTIL_OSKDIALOG_UNLOADED:
		break;

	case CELL_SYSUTIL_DRAWING_BEGIN:
	case CELL_SYSUTIL_DRAWING_END:
		break;
	case CELL_SYSUTIL_OSKDIALOG_INPUT_ENTERED:
		/* Get entered string */
		ret = cellOskDialogGetInputText( &OutputInfo );
		break;

	case CELL_SYSUTIL_OSKDIALOG_INPUT_DEVICE_CHANGED:
		if(param == CELL_OSKDIALOG_INPUT_DEVICE_KEYBOARD ){
		/* If the input device becomes the keyboard, */
		/*  stop receiving input from the onscreen keyboard dialog */
//			ret = cellOskDialogSetDeviceMask( CELL_OSKDIALOG_DEVICE_MASK_PAD );
		}
		break;

	case CELL_SYSUTIL_WEBBROWSER_SHUTDOWN_FINISHED:
		exit_flag_ = true;
		break;

	default:
		break;
	}
}

void draw_box( uint8_t *buffer_to, uint32_t width, uint32_t height, int x, int y, uint32_t border_color)
{
	int line = 1920 * 4;
	uint32_t pos_to_border = ( y * line) + (x * 4), cline=0;
	uint32_t lines=0;
	unsigned char* bt;

	for(lines=0; lines<(height); lines++)
	{
		for(cline=0; cline<(width*4); cline+=4)
		{
			bt = (uint8_t*)(buffer_to) + pos_to_border + cline;
			*(uint32_t*)bt = border_color;
		}
		pos_to_border+=line;
	}

}

void put_texture( uint8_t *buffer_to, uint8_t *buffer_from, uint32_t width, uint32_t height, int from_width, int x, int y, int border, uint32_t border_color)
{
	int row	 = from_width * 4;
	int line = 1920 * 4;
	uint32_t pos_to = ( y * line) + (x * 4), cline=0;
	uint32_t pos_to_border = ( (y-border) * line) + ((x-border) * 4);
	uint32_t pos_from = 0;
	uint32_t lines=0;
	unsigned char* bt;

	if(border)
	{
		for(lines=0; lines<(height+(border*2)); lines++)
		{
			for(cline=0; cline<((width+border*2)*4); cline+=4)
			{
				bt = (uint8_t*)(buffer_to) + pos_to_border + cline;
				*(uint32_t*)bt = border_color;
			}
			pos_to_border+=line;
		}
	}

	for(lines=0; lines<height; lines++)
	{

		memcpy(buffer_to + pos_to, buffer_from + pos_from, width * 4);
		pos_from+=row;
		pos_to+=line;
	}

}

void put_texture_with_alpha( uint8_t *buffer_to, uint8_t *buffer_from, uint32_t _width, uint32_t _height, int from_width, int x, int y, int border, uint32_t border_color)
{
	int row	 = from_width * 4;
	int line = 1920 * 4;
	uint32_t pos_to = ( y * line) + (x * 4), cline=0;
	uint32_t pos_to_border = ( (y-border) * line) + ((x-border) * 4);
	uint32_t pos_from = 0;
	uint32_t lines=0;
	uint32_t c_pixel_N_R, c_pixel_N_G, c_pixel_N_B;
	uint32_t c_pixelR, c_pixelG, c_pixelB, c_pixel_N_A, c_pixel;
	uint32_t width=_width;
	uint32_t height=_height;
	unsigned char* bt;
	unsigned char* btF;
	if( (x+width) > 1920) width=(1920-x);
	if( (y+height) > 1080) height=(1080-y);

	if(border)
	{
		for(lines=0; lines<(height+(border*2)); lines++)
		{
			for(cline=0; cline<((width+border*2)*4); cline+=4)
			{
				bt = (uint8_t*)(buffer_to) + pos_to_border + cline;
				*(uint32_t*)bt = border_color;
			}
			pos_to_border+=line;
		}
	}

	for(lines=0; lines<height; lines++)
	{
		for(cline=0; cline<((width)*4); cline+=4)
		{
			btF = (uint8_t*)(buffer_from) + pos_from + cline;
			bt = (uint8_t*)(buffer_to) + pos_to + cline;

			c_pixel = *(uint32_t*)btF;
			c_pixel_N_A = (c_pixel    ) & 0xff;

			if(c_pixel_N_A)
			{

				float d_alpha  = (c_pixel_N_A / 255.0f);
				float d_alpha1 = 1.0f-d_alpha;
				c_pixel_N_R = (int)(buffer_from[pos_from + cline + 0] * d_alpha);
				c_pixel_N_G = (int)(buffer_from[pos_from + cline + 1] * d_alpha);
				c_pixel_N_B = (int)(buffer_from[pos_from + cline + 2] * d_alpha);

				c_pixelR = (int)(buffer_to[pos_to + cline + 0] * d_alpha1) + c_pixel_N_R;
				c_pixelG = (int)(buffer_to[pos_to + cline + 1] * d_alpha1) + c_pixel_N_G;
				c_pixelB = (int)(buffer_to[pos_to + cline + 2] * d_alpha1) + c_pixel_N_B;

				//keep the higher alpha
				*(uint32_t*)bt = ((buffer_to[pos_to + cline + 3]>(c_pixel_N_A) ? buffer_to[pos_to + cline + 3] : (c_pixel_N_A) )) | (c_pixelR<<24) | (c_pixelG<<16) | (c_pixelB<<8);
			}
		}

		pos_from+=row;
		pos_to+=line;
	}

}

void put_texture_with_alpha_gen( uint8_t *buffer_to, uint8_t *buffer_from, uint32_t _width, uint32_t _height, int from_width, u16 to_width, int x, int y) //, int border, uint32_t border_color
{
	int row	 = from_width * 4;
	int line = to_width * 4;
	uint32_t pos_to = ( y * line) + (x * 4), cline=0;
//	uint32_t pos_to_border = ( (y-border) * line) + ((x-border) * 4);
	uint32_t pos_from = 0;
	uint32_t lines=0;
	uint32_t c_pixel_N_R, c_pixel_N_G, c_pixel_N_B;
	uint32_t c_pixelR, c_pixelG, c_pixelB, c_pixel_N_A, c_pixel;
	uint32_t width=_width;
	uint32_t height=_height;
	unsigned char* bt;
	unsigned char* btF;
	if( (x+width) > to_width) width=(to_width-x);
//	if( (y+height) > 1080) height=(1080-y);

/*	if(border)
	{
		for(lines=0; lines<(height+(border*2)); lines++)
		{
			for(cline=0; cline<((width+border*2)*4); cline+=4)
			{
				bt = (uint8_t*)(buffer_to) + pos_to_border + cline;
				*(uint32_t*)bt = border_color;
			}
			pos_to_border+=line;
		}
	}
*/
	for(lines=0; lines<height; lines++)
	{
		for(cline=0; cline<((width)*4); cline+=4)
		{
			btF = (uint8_t*)(buffer_from) + pos_from + cline;
			bt = (uint8_t*)(buffer_to) + pos_to + cline;

			c_pixel = *(uint32_t*)btF;
			c_pixel_N_A = (c_pixel    ) & 0xff;

			if(c_pixel_N_A)
			{

				float d_alpha  = (c_pixel_N_A / 255.0f);
				float d_alpha1 = 1.0f-d_alpha;
				c_pixel_N_R = (int)(buffer_from[pos_from + cline + 0] * d_alpha);
				c_pixel_N_G = (int)(buffer_from[pos_from + cline + 1] * d_alpha);
				c_pixel_N_B = (int)(buffer_from[pos_from + cline + 2] * d_alpha);

				c_pixelR = (int)(buffer_to[pos_to + cline + 0] * d_alpha1) + c_pixel_N_R;
				c_pixelG = (int)(buffer_to[pos_to + cline + 1] * d_alpha1) + c_pixel_N_G;
				c_pixelB = (int)(buffer_to[pos_to + cline + 2] * d_alpha1) + c_pixel_N_B;

				//keep the higher alpha
				*(uint32_t*)bt = ((buffer_to[pos_to + cline + 3]>(c_pixel_N_A) ? buffer_to[pos_to + cline + 3] : (c_pixel_N_A) )) | (c_pixelR<<24) | (c_pixelG<<16) | (c_pixelB<<8);
			}
		}

		pos_from+=row;
		pos_to+=line;
	}

}

void put_texture_VM_Galpha( uint8_t *buffer_to, uint32_t Twidth, uint32_t Theight, uint8_t *buffer_from, uint32_t _width, uint32_t _height, int from_width, int x, int y, int border, uint32_t border_color)
{
	int row	 = from_width * 4;
	int line = V_WIDTH * 4;
	uint32_t pos_to = ( y * line) + (x * 4), cline=0;
	uint32_t pos_to_border = ( (y-border) * line) + ((x-border) * 4);
	uint32_t pos_from = 0;
	uint32_t lines=0;
	uint32_t c_pixel_N_R, c_pixel_N_G, c_pixel_N_B;
	uint32_t c_pixelR, c_pixelG, c_pixelB, c_pixel_N_A, c_pixel;
	uint32_t width=_width-1;
	uint32_t height=_height-1;
	unsigned char* bt;
	unsigned char* btF;
	if( (x+width) > Twidth) width=(Twidth-x);
	if( (y+height) > Theight) height=(Theight-y);

	if(border)
	{
		for(lines=0; lines<(height+(border*2)); lines++)
		{
			for(cline=0; cline<((width+border*2)*4); cline+=4)
			{
				bt = (uint8_t*)(buffer_to) + pos_to_border + cline;
				*(uint32_t*)bt = border_color;
			}
			pos_to_border+=line;
		}
	}

	for(lines=0; lines<height; lines++)
	{
		for(cline=0; cline<((width)*4); cline+=4)
		{
			btF = (uint8_t*)(buffer_from) + pos_from + cline;
			bt = (uint8_t*)(buffer_to) + pos_to + cline;
			c_pixel = *(uint32_t*)btF;

			c_pixel_N_R = (c_pixel>>24) & 0xff;
			c_pixel_N_G = (c_pixel>>16) & 0xff;
			c_pixel_N_B = (c_pixel>>8) & 0xff;
			c_pixel_N_A = 255-c_pixel_N_G;

			if(c_pixel_N_B==0 && c_pixel_N_R==0)
			{

				float d_alpha  = (c_pixel_N_A / 255.0f);
				float d_alpha1 = 1.0f-d_alpha;
				//c_pixel_N_R = (int)(0x0 * d_alpha);
				//c_pixel_N_G = (int)(0x0 * d_alpha);
				//c_pixel_N_B = (int)(0x0 * d_alpha);

				c_pixelR = (int)(buffer_to[pos_to + cline + 1] * d_alpha1);// + c_pixel_N_R;
				c_pixelG = (int)(buffer_to[pos_to + cline + 2] * d_alpha1);// + c_pixel_N_G;
				c_pixelB = (int)(buffer_to[pos_to + cline + 3] * d_alpha1);// + c_pixel_N_B;

				*(uint32_t*)bt = 0xff000000 | (c_pixelR<<16) | (c_pixelG<<8) | c_pixelB;
			}
			else
				{
					*(uint32_t*)bt = 0xff000000 | (c_pixel>>8);// | (c_pixel_N_R<<16) | (c_pixel_N_G<<8) | c_pixel_N_B;
				}
		}

		pos_from+=row;
		pos_to+=line;
	}

}

void draw_mouse_pointer(int m_type)
{
	(void) m_type;
	put_texture_VM_Galpha( (uint8_t*)(color_base_addr)+video_buffer*frame_index, V_WIDTH, V_HEIGHT, mouse, mp_WIDTH, mp_HEIGHT, mp_WIDTH, (int)(mouseX*(float)V_WIDTH*(1.f-overscan)+overscan*(float)V_WIDTH*0.5f), (int)(mouseY*(float)V_HEIGHT*(1.f-overscan)+overscan*(float)V_HEIGHT*0.5f), 0, 0);
}


void put_texture_Galpha( uint8_t *buffer_to, uint32_t Twidth, uint32_t Theight, uint8_t *buffer_from, uint32_t _width, uint32_t _height, int from_width, int x, int y, int border, uint32_t border_color)
{
	int row	 = from_width * 4;
	int line = Twidth * 4;
	uint32_t pos_to = ( y * line) + (x * 4), cline=0;
	uint32_t pos_to_border = ( (y-border) * line) + ((x-border) * 4);
	uint32_t pos_from = 0;
	uint32_t lines=0;
	uint32_t c_pixel_N_R, c_pixel_N_G, c_pixel_N_B;
	uint32_t c_pixelR, c_pixelG, c_pixelB, c_pixel_N_A, c_pixel;
	uint32_t width=_width;
	uint32_t height=_height;
	unsigned char* bt;
	unsigned char* btF;
	if( (x+width) > Twidth) width=(Twidth-x);
	if( (y+height) > Theight) height=(Theight-y);

	if(border)
	{
		for(lines=0; lines<(height+(border*2)); lines++)
		{
			for(cline=0; cline<((width+border*2)*4); cline+=4)
			{
				bt = (uint8_t*)(buffer_to) + pos_to_border + cline;
				*(uint32_t*)bt = border_color;
			}
			pos_to_border+=line;
		}
	}

	for(lines=0; lines<height; lines++)
	{
		for(cline=0; cline<((width)*4); cline+=4)
		{
			btF = (uint8_t*)(buffer_from) + pos_from + cline;
			bt = (uint8_t*)(buffer_to) + pos_to + cline;
			c_pixel = *(uint32_t*)btF;

			c_pixel_N_R = (c_pixel>>24) & 0xff;
			c_pixel_N_G = (c_pixel>>16) & 0xff;
			c_pixel_N_B = (c_pixel>>8) & 0xff;
			c_pixel_N_A = 255-c_pixel_N_G;

			if(c_pixel_N_B==0 && c_pixel_N_R==0)
			{
//				if(c_pixel_N_G)
				{
					float d_alpha  = (c_pixel_N_A / 255.0f);
					float d_alpha1 = 1.0f-d_alpha;
					//c_pixel_N_R = (int)(0x0 * d_alpha);
					//c_pixel_N_G = (int)(0x0 * d_alpha);
					//c_pixel_N_B = (int)(0x0 * d_alpha);

					c_pixelR = (int)(buffer_to[pos_to + cline + 0] * d_alpha1);// + c_pixel_N_R;
					c_pixelG = (int)(buffer_to[pos_to + cline + 1] * d_alpha1);// + c_pixel_N_G;
					c_pixelB = (int)(buffer_to[pos_to + cline + 2] * d_alpha1);// + c_pixel_N_B;

					*(uint32_t*)bt = (c_pixelR<<24) | (c_pixelG<<16) | (c_pixelB<<8) | (c_pixel & 0xff) ;
				}
			}
			else
				{
					*(uint32_t*)bt = (c_pixel);// | 0xff);// | (c_pixel_N_R<<16) | (c_pixel_N_G<<8) | c_pixel_N_B;
				}
		}

		pos_from+=row;
		pos_to+=line;
	}

}

void put_reflection( uint8_t *buffer_to, uint32_t Twidth, uint32_t Theight, uint32_t _width, uint32_t _height, int x, int y, int dx, int dy, int factor)
{
	int row	 = Twidth * 4;
	int line = Twidth * 4;
	uint32_t pos_to =	( (dy + (_height/factor)) * line) + (dx * 4), cline=0;
	uint32_t pos_from = ( y * line) + (x * 4);

	uint32_t lines=0;
	uint32_t c_pixel_N_R, c_pixel_N_G, c_pixel_N_B;
	float c_pixel_N_RF, c_pixel_N_GF, c_pixel_N_BF;
	uint32_t c_pixelR, c_pixelG, c_pixelB, c_pixel_N_A, c_pixel;

	uint32_t width=_width;
	uint32_t height=_height;
	unsigned char* bt;
	unsigned char* btF;
	if( (dx+width) > Twidth) width=(Twidth-dx);
	if( (dy+height/factor) > Theight) height=(Theight-dy);


	for(lines=0; lines<height; lines+=factor)
	{
		for(cline=0; cline<((width)*4); cline+=4)
		{
			btF = (uint8_t*)(buffer_to) + pos_from + cline;
			bt = (uint8_t*)(buffer_to) + pos_to + cline;
			c_pixel = *(uint32_t*)btF;

			c_pixel_N_RF =  ( (c_pixel>>24) & 0xff ) * ( (float)(c_pixel & 0xff) / 255.0f) ;
			c_pixel_N_GF =  ( (c_pixel>>16) & 0xff ) * ( (float)(c_pixel & 0xff) / 255.0f) ;
			c_pixel_N_BF =  ( (c_pixel>>8) & 0xff ) * ( (float)(c_pixel & 0xff) / 255.0f) ;
			c_pixel_N_A = (uint32_t) ( (255.0f - ( 255.0f * ( (float)(lines/(float)height)) ))  );

			float d_alpha  = (c_pixel_N_A / 255.0f);
			float d_alpha1 = (1.0f-d_alpha)/2.0f;
			d_alpha= 1.0f - d_alpha1;
			c_pixel_N_R = (int)(buffer_to[pos_to + cline + 0] * d_alpha);
			c_pixel_N_G = (int)(buffer_to[pos_to + cline + 1] * d_alpha);
			c_pixel_N_B = (int)(buffer_to[pos_to + cline + 2] * d_alpha);

			c_pixelR = (int)(c_pixel_N_RF * d_alpha1) + c_pixel_N_R;
			c_pixelG = (int)(c_pixel_N_GF * d_alpha1) + c_pixel_N_G;
			c_pixelB = (int)(c_pixel_N_BF * d_alpha1) + c_pixel_N_B;

			*(uint32_t*)bt = (c_pixelR<<24) | (c_pixelG<<16) | (c_pixelB<<8) | (0xff) ;

		}

		pos_from+=(row*factor);
		pos_to-=line;
	}

}


/*
void alter_texture( uint8_t *buffer_to, uint8_t *buffer_from, uint32_t width, uint32_t height, int x, int y, int border, uint32_t border_color)
{
	int row	 = 320 * 4;
	int line = 1920 * 4;
	uint32_t pos_to = ( y * line) + (x * 4), cline=0;
	uint32_t pos_to_border = ( (y-border) * line) + ((x-border) * 4);
	uint32_t pos_from = 0;
	uint32_t lines=0;
	uint16_t c_pixel, c_pixelR, c_pixelG, c_pixelB, c_pixelR_AVG, c_pixelG_AVG, c_pixelB_AVG, c_BRI;

	c_BRI=0; //brightness decrease
	int use_grayscale=0;
	int use_blur=0;

	if(border)
	{
		for(lines=0; lines<(height+(border*2)); lines++)
		{
			for(cline=0; cline<((width+border*2)*4); cline+=4)
			{
				memset(buffer_to + pos_to_border + cline + 0, (border_color>> 8) & 0xff, 1);
				memset(buffer_to + pos_to_border + cline + 1, (border_color>>16) & 0xff, 1);
				memset(buffer_to + pos_to_border + cline + 2, (border_color>>24) & 0xff, 1);
				memset(buffer_to + pos_to_border + cline + 3, (border_color    ) & 0xff, 1);
			}
			pos_to_border+=line;
		}
	}

	for(lines=0; lines<height; lines++)
	{

		if(!use_blur && !use_grayscale)
			memcpy(buffer_to + pos_to, buffer_from + pos_from, width * 4);
		else

		for(cline=0; cline<(width*4); cline+=4) 
		{

			if(use_blur)
			{
			// box blur
			if(lines>0 && cline>0 && lines<(height-1) && cline<((width-1)*4))
			{
				c_pixelB = buffer_from[pos_from + cline + 0 + 4];
				c_pixelG = buffer_from[pos_from + cline + 1 + 4];
				c_pixelR = buffer_from[pos_from + cline + 2 + 4];

				c_pixelB+= buffer_from[pos_from + cline + 0 - 4];
				c_pixelG+= buffer_from[pos_from + cline + 1 - 4];
				c_pixelR+= buffer_from[pos_from + cline + 2 - 4];

				c_pixelB+= buffer_from[pos_from + cline + 0 - row];
				c_pixelG+= buffer_from[pos_from + cline + 1 - row];
				c_pixelR+= buffer_from[pos_from + cline + 2 - row];

				c_pixelB+= buffer_from[pos_from + cline + 0 + row];
				c_pixelG+= buffer_from[pos_from + cline + 1 + row];
				c_pixelR+= buffer_from[pos_from + cline + 2 + row];

				c_pixelB+= buffer_from[pos_from + cline + 0 - row - 4];
				c_pixelG+= buffer_from[pos_from + cline + 1 - row - 4];
				c_pixelR+= buffer_from[pos_from + cline + 2 - row - 4];

				c_pixelB+= buffer_from[pos_from + cline + 0 + row + 4];
				c_pixelG+= buffer_from[pos_from + cline + 1 + row + 4];
				c_pixelR+= buffer_from[pos_from + cline + 2 + row + 4];

				c_pixelB+= buffer_from[pos_from + cline + 0 - row + 4];
				c_pixelG+= buffer_from[pos_from + cline + 1 - row + 4];
				c_pixelR+= buffer_from[pos_from + cline + 2 - row + 4];

				c_pixelB+= buffer_from[pos_from + cline + 0 + row - 4];
				c_pixelG+= buffer_from[pos_from + cline + 1 + row - 4];
				c_pixelR+= buffer_from[pos_from + cline + 2 + row - 4];

				// average values
				c_pixelB_AVG=((uint8_t) (c_pixelB/8));
				c_pixelG_AVG=((uint8_t) (c_pixelG/8));
				c_pixelR_AVG=((uint8_t) (c_pixelR/8));

				if(c_BRI>0) 
				{
					if(c_pixelB_AVG>c_BRI) c_pixelB_AVG-=c_BRI; else c_pixelB_AVG=0;
					if(c_pixelG_AVG>c_BRI) c_pixelG_AVG-=c_BRI; else c_pixelG_AVG=0;
					if(c_pixelR_AVG>c_BRI) c_pixelR_AVG-=c_BRI; else c_pixelR_AVG=0;
				}

				if(use_grayscale)
				{
					// greyscale + box blur
					c_pixel = c_pixelB_AVG + c_pixelG_AVG + c_pixelR_AVG;
					memset(buffer_to + pos_to + cline + 0, (uint8_t) (c_pixel/3), 3);
				}
				else
				{
					memset(buffer_to + pos_to + cline + 0, c_pixelB_AVG, 1);
					memset(buffer_to + pos_to + cline + 1, c_pixelG_AVG, 1);
					memset(buffer_to + pos_to + cline + 2, c_pixelR_AVG, 1);
				}

			}

			else 
				{
					// convert to grayscale only
					c_pixel = buffer_from[pos_from + cline + 0];
					c_pixel+= buffer_from[pos_from + cline + 1];
					c_pixel+= buffer_from[pos_from + cline + 2];
					if(c_BRI>0) { if(c_pixel>(c_BRI*3)) c_pixel-=(c_BRI*3); else c_pixel=0; }
					memset(buffer_to + pos_to + cline + 0, (uint8_t) (c_pixel/3), 3);
				}
			} //use blur

			else 

			{
				// convert to grayscale only
				c_pixel = buffer_from[pos_from + cline + 0];
				c_pixel+= buffer_from[pos_from + cline + 1];
				c_pixel+= buffer_from[pos_from + cline + 2];
				if(c_BRI>0) { if(c_pixel>(c_BRI*3)) c_pixel-=(c_BRI*3); else c_pixel=0; }
				memset(buffer_to + pos_to + cline + 0, (uint8_t) (c_pixel/3), 3);
			}


			// keep alpha
			memset(buffer_to + pos_to + cline + 3, buffer_from[pos_from + cline + 3], 1);
		}


		pos_from+=row;
		pos_to+=line;
	}

}
*/

void gray_texture( uint8_t *buffer_to, uint32_t width, uint32_t height, int step)
{
	if(gray_poster==0) return;
	uint32_t cline=0;
	uint16_t c_pixel;
	int line=0;
	(void) step;
	(void) line;

	for(cline=0; cline<(width*height*4); cline+=4) 
	{
		/*if(step){
			line++;
			if(line>=width) {
				line=0;
				memset(buffer_to + cline, 0, width*4);
				cline+=width*4;
				continue;
			}
		}*/
		c_pixel = buffer_to[cline];
		c_pixel+= buffer_to[cline + 1];
		c_pixel+= buffer_to[cline + 2];
		memset(buffer_to + cline, (uint8_t) (c_pixel/3), 3);
	}
}

/*
void to_565_texture( uint8_t *buffer_from, uint8_t *buffer_to, uint32_t width, uint32_t height)
{
	uint32_t cline=0, pos_to=0;//, height=sizeof(buffer_from)/4/width;
	uint8_t c_pixelR, c_pixelG, c_pixelB, c_pixel1, c_pixel2;


	for(cline=0; cline<(width*height*4); cline+=4) 
	{
		c_pixelR = buffer_from[cline];
		c_pixelG = buffer_from[cline + 1];
		c_pixelB = buffer_from[cline + 2];

		c_pixel1 = (c_pixelR & 0xF8) | ( (c_pixelG & 0xE0) >> 5);
		c_pixel2 = ( (c_pixelG & 0x1C) << 3) | (c_pixelB >> 3);

		memset(buffer_to + pos_to, c_pixel1, 1);
		memset(buffer_to + pos_to+1, c_pixel2, 1);
		pos_to+=2;
	}
}

void to_RGB_texture( uint8_t *buffer_from, uint8_t *buffer_to, uint32_t width, uint32_t height)
{
	uint32_t cline=0, pos_to=0;//, height=sizeof(buffer_from)/4/width;
	uint8_t c_pixelR, c_pixelG, c_pixelB, c_pixel1, c_pixel2;


	for(cline=0; cline<(width*height*2); cline+=2) 
	{
		c_pixel1 = buffer_from[cline];
		c_pixel2 = buffer_from[cline + 1];

		c_pixelR = (c_pixel1 & 0xF8);
		c_pixelG = ( (c_pixel1 & 0x7) << 5) | ((c_pixel2 & 0xE0)>>5);
		c_pixelB = (c_pixel2 & 0x1f)<<3;

		memset(buffer_to + pos_to, c_pixelR, 1);
		memset(buffer_to + pos_to+1, c_pixelG, 1);
		memset(buffer_to + pos_to+2, c_pixelB, 1);
		memset(buffer_to + pos_to+3, 0xff, 1);
		pos_to+=4;
	}
}

void to_333_texture( uint8_t *buffer_from, uint8_t *buffer_to, uint32_t width, uint32_t height)
{
	uint32_t cline=0, pos_to=0;//, height=sizeof(buffer_from)/4/width;

	for(cline=0; cline<(width*height*4); cline+=4) 
	{
		memcpy(buffer_to + pos_to, buffer_from + cline, 3);
//		memcpy(buffer_to + pos_to, buffer_to + cline, 3);
		pos_to+=3;
	}
}

void to_RGB3_texture( uint8_t *buffer_from, uint8_t *buffer_to, uint32_t width, uint32_t height)
{
	uint32_t cline=0, pos_to=0;

	for(cline=0; cline<(width*height*3); cline+=3) 
	{
		memcpy(buffer_to + pos_to, buffer_from + cline, 3);
		memset(buffer_to + pos_to + 3, 0xff, 1);
		pos_to+=4;
	}
}

*/
void mip_texture( uint8_t *buffer_to, uint8_t *buffer_from, uint32_t width, uint32_t height, int scaleF)
{
	uint32_t pos_to = 0, pos_from = 0, cline=0, scale, cscale;
	uint32_t lines=0;

	if(scaleF<0)
	{
		scale=(-1)*scaleF;
		for(lines=0; lines<height; lines+=scale)
		{
			pos_from = lines * width * 4;
			for(cline=0; cline<(width*4); cline+=(4*scale)) 
			{
				memcpy(buffer_to + pos_to, buffer_from + pos_from + cline, 4);
				pos_to+=4;
			}
		}
	}
	else
	{
		scale=scaleF;

		for(lines=0; lines<height; lines++)
		{
			pos_from = lines * width * 4;
			for(cline=0; cline<(width*4); cline+=4) 
			{
				for(cscale=0; cscale<scale; cscale++) 
				{
					memcpy(buffer_to + pos_to, buffer_from + pos_from + cline, 4);
					pos_to+=4;
				}
			}

			for(cscale=0; cscale<(scale-1); cscale++) 
			{
				memcpy(buffer_to + pos_to, buffer_to + pos_to - width * scale * 4, width * scale * 4);
				pos_to+=width * scale * 4;
			}
		}

	}

}

void blur_texture(uint8_t *buffer_to, uint32_t width, uint32_t height, int x, int y,  int wx, int wy, uint32_t c_BRI, int use_grayscale, int iterations, int p_range)
{

	int p_step = 4 * p_range;
	int row	 = width * p_step;

	int line = width * 4;
	uint32_t pos_to=0;
	int lines=0, cline=0, iter=0;
	(void) height;
	uint32_t c_pixel, c_pixelR, c_pixelG, c_pixelB, c_pixelR_AVG, c_pixelG_AVG, c_pixelB_AVG;
	int use_blur=1;

	if(iterations==0) {use_blur=0; iterations=1;}

	for(iter=0; iter<iterations; iter++) 
	{
	pos_to = ( y * line) + (x * 4);

	for(lines=0; lines<wy; lines++)
	{

		for(cline=0; cline<(wx*4); cline+=4) 
		{

			if(lines>=p_range && cline>=p_range && lines<(wy-p_range) && cline<((wx-p_range)*4))
			{

/*
				bt = (uint8_t*)(buffer_to) + pos_to + cline;
				c_pixel  = (*(uint32_t*)(bt + p_step))>>8;
				c_pixelB = (c_pixel>>16)&0xff;
				c_pixelG = (c_pixel>> 8)&0xff;
				c_pixelR =  c_pixel&0xff;

				c_pixel= (*(uint32_t*)(bt - p_step))>>8;
				c_pixelB+= (c_pixel>>16)&0xff;
				c_pixelG+= (c_pixel>> 8)&0xff;
				c_pixelR+=  c_pixel&0xff;

				c_pixel= (*(uint32_t*)(bt - row))>>8;
				c_pixelB+= (c_pixel>>16)&0xff;
				c_pixelG+= (c_pixel>> 8)&0xff;
				c_pixelR+=  c_pixel&0xff;

				c_pixel= (*(uint32_t*)(bt + row))>>8;
				c_pixelB+= (c_pixel>>16)&0xff;
				c_pixelG+= (c_pixel>> 8)&0xff;
				c_pixelR+=  c_pixel&0xff;

				c_pixel= (*(uint32_t*)(bt - row - p_step))>>8;
				c_pixelB+= (c_pixel>>16)&0xff;
				c_pixelG+= (c_pixel>> 8)&0xff;
				c_pixelR+=  c_pixel&0xff;

				c_pixel= (*(uint32_t*)(bt + row + p_step))>>8;
				c_pixelB+= (c_pixel>>16)&0xff;
				c_pixelG+= (c_pixel>> 8)&0xff;
				c_pixelR+=  c_pixel&0xff;

				c_pixel= (*(uint32_t*)(bt - row + p_step))>>8;
				c_pixelB+= (c_pixel>>16)&0xff;
				c_pixelG+= (c_pixel>> 8)&0xff;
				c_pixelR+=  c_pixel&0xff;

				c_pixel= (*(uint32_t*)(bt + row - p_step))>>8;
				c_pixelB+= (c_pixel>>16)&0xff;
				c_pixelG+= (c_pixel>> 8)&0xff;
				c_pixelR+=  c_pixel&0xff;

//				*(uint32_t*)bt = c_pixel;
*/

			if(use_blur)
			{
			// box blur
				// get RGB values for all surrounding pixels
				// to create average for blurring
				c_pixelB = buffer_to[pos_to + cline + 0 + p_step];
				c_pixelG = buffer_to[pos_to + cline + 1 + p_step];
				c_pixelR = buffer_to[pos_to + cline + 2 + p_step];

				c_pixelB+= buffer_to[pos_to + cline + 0 - p_step];
				c_pixelG+= buffer_to[pos_to + cline + 1 - p_step];
				c_pixelR+= buffer_to[pos_to + cline + 2 - p_step];

				c_pixelB+= buffer_to[pos_to + cline + 0 - row];
				c_pixelG+= buffer_to[pos_to + cline + 1 - row];
				c_pixelR+= buffer_to[pos_to + cline + 2 - row];

				c_pixelB+= buffer_to[pos_to + cline + 0 + row];
				c_pixelG+= buffer_to[pos_to + cline + 1 + row];
				c_pixelR+= buffer_to[pos_to + cline + 2 + row];

				c_pixelB+= buffer_to[pos_to + cline + 0 - row - p_step];
				c_pixelG+= buffer_to[pos_to + cline + 1 - row - p_step];
				c_pixelR+= buffer_to[pos_to + cline + 2 - row - p_step];

				c_pixelB+= buffer_to[pos_to + cline + 0 + row + p_step];
				c_pixelG+= buffer_to[pos_to + cline + 1 + row + p_step];
				c_pixelR+= buffer_to[pos_to + cline + 2 + row + p_step];

				c_pixelB+= buffer_to[pos_to + cline + 0 - row + p_step];
				c_pixelG+= buffer_to[pos_to + cline + 1 - row + p_step];
				c_pixelR+= buffer_to[pos_to + cline + 2 - row + p_step];

				c_pixelB+= buffer_to[pos_to + cline + 0 + row - p_step];
				c_pixelG+= buffer_to[pos_to + cline + 1 + row - p_step];
				c_pixelR+= buffer_to[pos_to + cline + 2 + row - p_step];

				// average values
				c_pixelB_AVG=((uint8_t) (c_pixelB/8));
				c_pixelG_AVG=((uint8_t) (c_pixelG/8));
				c_pixelR_AVG=((uint8_t) (c_pixelR/8));
			}
				else //no blur
				{
					c_pixelB_AVG = buffer_to[pos_to + cline + 0];
					c_pixelG_AVG = buffer_to[pos_to + cline + 1];
					c_pixelR_AVG = buffer_to[pos_to + cline + 2];
				}


/*				if(c_BRI>0 && c_BRI<100) // decrease brightness
				{
					if(c_pixelB_AVG>c_BRI) c_pixelB_AVG-=c_BRI; else c_pixelB_AVG=0;
					if(c_pixelG_AVG>c_BRI) c_pixelG_AVG-=c_BRI; else c_pixelG_AVG=0;
					if(c_pixelR_AVG>c_BRI) c_pixelR_AVG-=c_BRI; else c_pixelR_AVG=0;
				}*/

				if(c_BRI>0) // increase brightnes by percent (101+=1%+)
				{
					c_pixelB_AVG=(uint32_t) (c_pixelB_AVG*(((float)c_BRI)/100.0f) ); if(c_pixelB_AVG>0xff) c_pixelB_AVG=0xff;
					c_pixelG_AVG=(uint32_t) (c_pixelG_AVG*(((float)c_BRI)/100.0f) ); if(c_pixelG_AVG>0xff) c_pixelG_AVG=0xff;
					c_pixelR_AVG=(uint32_t) (c_pixelR_AVG*(((float)c_BRI)/100.0f) ); if(c_pixelR_AVG>0xff) c_pixelR_AVG=0xff;

				}

				if(use_grayscale)
				{
					// greyscale + box blur
					c_pixel = c_pixelB_AVG + c_pixelG_AVG + c_pixelR_AVG;
					memset(buffer_to + pos_to + cline + 0, (uint8_t) (c_pixel/3), 3);
				}
				else
				{
					buffer_to[pos_to + cline	]= c_pixelB_AVG;
					buffer_to[pos_to + cline + 1]= c_pixelG_AVG;
					buffer_to[pos_to + cline + 2]= c_pixelR_AVG;
				}
			}
			else
			{
				c_pixelB_AVG = buffer_to[pos_to + cline + 0];
				c_pixelG_AVG = buffer_to[pos_to + cline + 1];
				c_pixelR_AVG = buffer_to[pos_to + cline + 2];
				if(c_BRI>0) // increase brightnes by percent (101+=1%+)
				{
					c_pixelB_AVG=(uint32_t) (c_pixelB_AVG*(((float)c_BRI)/100.0f) ); if(c_pixelB_AVG>0xff) c_pixelB_AVG=0xff;
					c_pixelG_AVG=(uint32_t) (c_pixelG_AVG*(((float)c_BRI)/100.0f) ); if(c_pixelG_AVG>0xff) c_pixelG_AVG=0xff;
					c_pixelR_AVG=(uint32_t) (c_pixelR_AVG*(((float)c_BRI)/100.0f) ); if(c_pixelR_AVG>0xff) c_pixelR_AVG=0xff;

				}

				if(use_grayscale)
				{
					// greyscale + box blur
					c_pixel = c_pixelB_AVG + c_pixelG_AVG + c_pixelR_AVG;
					memset(buffer_to + pos_to + cline + 0, (uint8_t) (c_pixel/3), 3);
				}
				else
				{	buffer_to[pos_to + cline	]= c_pixelB_AVG;
					buffer_to[pos_to + cline + 1]= c_pixelG_AVG;
					buffer_to[pos_to + cline + 2]= c_pixelR_AVG;
				}

			}


			if(use_grayscale && !use_blur) 
			{
				// convert to grayscale only
				c_pixel = buffer_to[pos_to + cline + 0];
				c_pixel+= buffer_to[pos_to + cline + 1];
				c_pixel+= buffer_to[pos_to + cline + 2];
				if(c_BRI>0) 
					{ if(c_pixel>(c_BRI*3)) 
					c_pixel-=(c_BRI*3); else c_pixel=0; }
				memset(buffer_to + pos_to + cline, (uint8_t) (c_pixel/3), 3);
			}


			// keep alpha
			// memset(buffer_to + pos_to + cline + 3, buffer_to[pos_to + cline + 3], 1);
			//if(sub_menu_open) 
			//buffer_to[pos_to + cline + 3] = 0x80;
		}

		pos_to+=line;
	}
	}//iterations
}



void draw_list_text( uint8_t *buffer, uint32_t width, uint32_t height, t_menu_list *menu, 
			int menu_size, int selected, int _dir_mode, int _display_mode, int _cover_mode, int opaq, int to_draw )
{
//uint8_t *buffer = NULL;
//buffer=(uint8_t*)(color_base_addr)+video_buffer*frame_index;
	if(to_draw && (_cover_mode==0))	memset(buffer, 0x00, (1920*1080*4));
	float y = 0.1f, yb;
	int i = 0, c=0;
	char str[256];
	char ansi[256];
	char is_split[8]; 
	float len=0;
	u32 color, color2;
	game_sel_last+=0;
	int flagb= selected & 0x10000;
	int max_entries=14;

	yb=y;
	selected&= 0xffff;

	if(!to_draw) {
	while( (c<max_entries && i < menu_size) ) 
	{

		if( (_display_mode==1 && strstr(menu[i].content,"AVCHD")!=NULL) || (_display_mode==2 && strstr(menu[i].content,"PS3")!=NULL) ) { i++; continue;}

		if( (i >= (int) (selected / max_entries)*max_entries) )
		{
			{

		len=1.18f;
		if(i==selected){
			u32 b_color=0x0080ffd0;
			b_box_opaq+=b_box_step;
			if(_cover_mode==2){
				if(b_box_opaq>0xfb) b_box_step=-4;
				if(b_box_opaq<0x20) b_box_step= 8;
			}
			else{
				if(b_box_opaq>0xc0) b_box_step=-2;
				if(b_box_opaq<0x30) b_box_step= 1;
			}

			b_color = (b_color & 0xffffff00) | (b_box_opaq-20);
//			draw_square((0.08f+_overscan-0.5f)*2.0f-0.02f, (0.5f-y+0.01)*2.0f , len+0.04f, 0.1f, 0.3f, 0x0080ff80);
			if(_cover_mode==2)
			{
//				draw_square((0.08f+_overscan-0.5f)*2.0f-0.02f, (0.5f-y+0.01)*2.0f , len+0.04f, 0.1f, -0.5f, 0x0080ff40);
				draw_square((0.08f-0.5f)*2.0f-0.02f, (0.5f-y+0.01)*2.0f , len+0.04f, 0.006f, -0.5f, b_color);
				b_color = (b_color & 0xffffff00) | b_box_opaq;
				draw_square((0.08f-0.5f)*2.0f-0.02f, (0.5f-y-0.038)*2.0f , len+0.04f, 0.006f, -0.5f, b_color);
				//draw_square(-0.9f+(overscan/2), 0.9f-(overscan/2), 2.0f-(0.70f-(overscan*2)), 2.0-0.50f-(2.0*overscan), -1.0f, 0x20202040);
			}
			else
				draw_square((0.08f-0.5f)*2.0f-0.02f, (0.5f-y+0.01)*2.0f , len+0.04f, 0.1f, 0.0f, b_color);
			break;
		}

		y += 0.05f;
		c++;
		}
		}
		i++; 

	}
		// bottom device icon
		if(th_drive_icon==1)
		{
			if(strstr(menu[selected].path, "/dev_usb")!=NULL || strstr(menu[selected].path, "/pvd_usb")!=NULL)
			{
				put_texture( buffer, text_USB, 96, 96, 320, th_drive_icon_x, th_drive_icon_y, 0, 0x0080ff80);
			}
			else if(strstr(menu[selected].path, "/dev_hdd")!=NULL)
			{
				put_texture( buffer, text_HDD, 96, 96, 320, th_drive_icon_x, th_drive_icon_y, 0, 0xff800080);
			}
			else if(strstr(menu[selected].path, "/dev_bdvd")!=NULL)
			{
				put_texture( buffer, text_BLU_1, 96, 96, 320, th_drive_icon_x, th_drive_icon_y, 0, 0xff800080);
			}
		}

	return;
	}

		CellFontRenderer* renderer;
		CellFontRenderSurface* surf;
		CellFont Font[1];
		CellFont* cf;
		int fn;
		int ret;
		int i_offset=0;
		
		surf     = &RenderWork.Surface;
		cellFontRenderSurfaceInit( surf, 
		                           buffer, width*4, 4,
		                           width, height );
		if(_cover_mode==2 || _cover_mode==0)
			cellFontRenderSurfaceSetScissor( surf, 0, 0, (int)(0.62f*width)+(int)((0.05f)*width), height );
		else
			cellFontRenderSurfaceSetScissor( surf, 0, 0, width, height );

		renderer = &RenderWork.Renderer;
		fn = FONT_SYSTEM_GOTHIC_JP;
		if(user_font<2 || user_font>9) fn = FONT_SYSTEM_GOTHIC_JP;
		else if (user_font==2) fn = FONT_SYSTEM_GOTHIC_LATIN;
		else if (user_font==3) fn = FONT_SYSTEM_SANS_SERIF;
		else if (user_font==4) fn = FONT_SYSTEM_SERIF;
		else if (user_font>4 && user_font<10) fn=user_font+5;

/*		if(user_font==0) fn = FONT_USER_FONT0;		

		if(user_font==5) fn = FONT_SYSTEM_5;
		if(user_font==6) fn = FONT_SYSTEM_6;
		if(user_font==7) fn = FONT_SYSTEM_7;
		if(user_font==8) fn = FONT_SYSTEM_8;
		if(user_font==9) fn = FONT_SYSTEM_9; */

//		fn = FONT_SYSTEM_GOTHIC_LATIN;
//		fn = FONT_SYSTEM_SANS_SERIF;
//		fn = FONT_SYSTEM_SERIF;
//		fn = FONT_USER_FONT0;
		
		

		ret = Fonts_AttachFont( fonts, fn, &Font[0] );
		if ( ret == CELL_OK ) cf = &Font[0];
		else                  cf = (CellFont*)0;

		if ( cf ) {

			static float weight = 1.04f;
			static float slant = 0.08f;
			float scale;
			float step;
			float lineH, baseY;

//			char c_ver[64]; sprintf(c_ver, "ver. %s", current_version);// (%02d/%02d/%04d %02d:%02d), timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
//			uint8_t* c_entry;//"ver 01.10.12";
//			float x=0.08f;

			step  =  0.f;
			scale = 24.f;

			ret = Fonts_SetFontScale( cf, scale );
			ret = Fonts_SetFontEffectWeight( cf, weight );
			ret = Fonts_SetFontEffectSlant( cf, slant );
			ret = Fonts_GetFontHorizontalLayout( cf, &lineH, &baseY );
				
			Fonts_BindRenderer( cf, renderer );

int it;
for(it=0;it<2;it++)
	{
	y=yb;
	i = 0; c=0;
	while( (c<max_entries && i < menu_size) ) 
	{

		if( (_display_mode==1 && strstr(menu[i].content,"AVCHD")!=NULL) || (_display_mode==2 && strstr(menu[i].content,"PS3")!=NULL) ) { i++; continue;}

		if( (i >= (int) (selected / max_entries)*max_entries) )
		{

		int grey=0;
		is_split[0]=0;
		if(i<menu_size)
			{
//			utf8_to_ansi(menu[i].title, ansi, 128);
			grey=0;
			if(menu[i].title[0]=='_')
				{ sprintf(ansi, "%s", menu[i].title+1); grey=1; sprintf(is_split, " (Split)");}
			else
				sprintf(ansi, "%s", menu[i].title);

			if(_dir_mode==0 && (_cover_mode==0 || _cover_mode==2)) { ansi[64]=0; }
			if(_dir_mode!=0 && (_cover_mode==0 || _cover_mode==2)) { ansi[64]=0; }
			if(_dir_mode==0 && _cover_mode==3) { ansi[47]=0; }
			if(_dir_mode!=0 && _cover_mode==3) { ansi[62]=0; }
			if( (_cover_mode==1 || _cover_mode==4 || _cover_mode==7)) { ansi[128]=0; }
			sprintf(str, "%s%s", ansi, is_split );
			}
		else
			{
			sprintf(str, " ");
			}
		
//		color= 0xffffffff;

		if(i==selected)
			color= (flagb && i==0) ? COL_PS3DISCSEL : ((grey==0) ? COL_SEL : 0xff008080);

		else {
			color= (flagb && i==0)? COL_PS3DISC : ((grey==0) ?  COL_PS3 : COL_SPLIT);// 0xd0ffffff
			if(strstr(menu[i].content,"AVCHD")!=NULL) color=COL_AVCHD;
			if(strstr(menu[i].content,"BDMV")!=NULL) color=COL_BDMV;
			if(strstr(menu[i].content,"PS2")!=NULL) color=COL_PS2;
			if(strstr(menu[i].content,"DVD")!=NULL) color=COL_DVD;
			}

//		color2=( (color & 0x00ffffff) | (opaq<<24));
		color2=color;
//		color= opaq<<16 | opaq<<8 | opaq;


		if(i==selected)	color2 = 0xffffffff;// else color2 = 0x17e8e8e8;
//		color2 = 0x00fefefe;
//		if(i!=selected && _cover_mode==0) color2 = 0x17e8e8e8;

//		color = (color & 0x00ffffff) | (((color>>24)/2)<<24);
		color = 0xff101010;
		ret = Fonts_SetFontEffectSlant( cf, 0.1f );
		if(strstr(menu[i].content, "PS3")!=NULL) i_offset=100; else i_offset=0;
		if(_dir_mode!=0)
		{	len=0.023f*(float)(strlen(str)+2);

			{
			if(_dir_mode==1)
			{
				if(_cover_mode!=0 && it==0) Fonts_RenderPropText( cf, surf, (int)((0.08f)*1920)+1+i_offset, (int)((y-0.005f)*1080)+1, (uint8_t*) str, scale*1.1f, scale, slant, step, color );
				if(_cover_mode==0 || it==1) Fonts_RenderPropText( cf, surf, (int)((0.08f)*1920)+i_offset, (int)((y-0.005f)*1080), (uint8_t*) str, scale*1.1f, scale, slant, step, color2 );
			}
			else
			{
				if(_cover_mode!=0 && it==0) Fonts_RenderPropText( cf, surf, (int)((0.08f)*1920)+2+i_offset, (int)((y+0.001f)*1080)+2, (uint8_t*) str, scale*1.3f, scale, slant, step, color );
				if(_cover_mode==0 || it==1) Fonts_RenderPropText( cf, surf, (int)((0.08f)*1920)+i_offset, (int)((y+0.001f)*1080), (uint8_t*) str, scale*1.3f, scale, slant, step, color2 );
			}

			}

		}

		else
		{	len=0.03f*(float)(strlen(str));
			if(opaq>0x020 || i==selected)
			{

				if(_cover_mode!=0 && it==0) Fonts_RenderPropText( cf, surf, (int)((0.08f)*1920)+2+i_offset, (int)((y-0.005f)*1080)+2, (uint8_t*) str, scale*1.5f, scale*1.4f, slant, step, color );
				if(_cover_mode==0 || it==1) Fonts_RenderPropText( cf, surf, (int)((0.08f)*1920)+i_offset, (int)((y-0.005f)*1080), (uint8_t*) str, scale*1.5f, scale*1.4f, slant, step, color2 );
			}
		}

		if(strlen(str)>1 && _dir_mode==1)
		{	
			sprintf(str, "%s", menu[i].path);
			if(strstr(menu[i].content,"AVCHD")!=NULL || strstr(menu[i].content,"BDMV")!=NULL)
				sprintf(str, "(%s) %s", menu[i].entry, menu[i].details); str[102]=0;

			if(0.01125f*(float)(strlen(str))>len) len=0.01125f*(float)(strlen(str));
			if(opaq>0x020 || i==selected)
				{
				if(_cover_mode!=0 && it==0) Fonts_RenderPropText( cf, surf, (int)((0.08f)*1920)+1+i_offset, (int)((y+0.022f)*1080)+1, (uint8_t*) str, scale/1.4f, scale/2.0f, 0.0f, step, color );
				if(_cover_mode==0 || it==1) Fonts_RenderPropText( cf, surf, (int)((0.08f)*1920)+i_offset, (int)((y+0.022f)*1080), (uint8_t*) str, scale/1.4f, scale/2.0f, 0.0f, step, color2 );
				}
		}

		if((_cover_mode==0 || it==1) && strstr(menu[i].content, "PS3")!=NULL && strstr(menu[i].title_id, "NO_ID")==NULL)
		{
//			Fonts_RenderPropText( cf, surf, (int)((0.08f+_overscan)*1920), (int)((y-0.005f)*1080), (uint8_t*) str, scale*1.5f, scale*1.4f, slant, step, color2 );
			sprintf(str, "%s/%s_80.RAW", cache_dir, menu[i].title_id);
			if(load_raw_texture( (u8*)buf+1024*1024, str, 80))
//				put_texture( buffer, (u8*)buf+1024*1024, 80, 45, 80, (int)((0.08f+_overscan)*1920)+1045, (int)((y-0.005f)*1080), 1, 0x0080ff80);
				put_texture( buffer, (u8*)buf+1024*1024, 80, 45, 80, (int)((0.08f)*1920), (int)((y-0.005f)*1080), 1, 0x80808080);
		}

		len=1.18f;
		y += 0.05f;
		c++;
		}
		i++; 

	}
	if(_cover_mode==0) break;

	if((_cover_mode==2 && it==0))
	{
		if(menu[selected].title[0]=='_' || menu_list[selected].split) 
		{
			gray_texture(buffer, 1920, 1080, 1);
			blur_texture(buffer, 1920, 1080, (int)((0.05f)*1920), (int) ((0.1f-0.025f)*1080), 1250, (int)((0.05f*max_entries+0.025f)*1080),  55, 0, 0, 2);
		}
		else
			blur_texture(buffer, 1920, 1080, (int)((0.05f)*1920), (int) ((0.1f-0.025f)*1080), 1250, (int)((0.05f*max_entries+0.025f)*1080),  60, 0, 0, 2);

//		int it;
//		for(it=3;it<21;it+=3)
//			blur_texture(buffer, 1920, 1080, (int)((0.05f+_overscan)*1920)+it, (int) ((_overscan+0.1f-0.025f)*1080)+it, 1250-(it*2), (int)((0.05f*max_entries+0.025f)*1080)-(it*2),   1, 0, 1);
//		for(it=3;it<12;it+=3)
//		it=12;
//			blur_texture(buffer, 1920, 1080, (int)((0.05f+_overscan)*1920)+it, (int) ((_overscan+0.1f-0.025f)*1080)+it, 1250-(it*2), (int)((0.05f*max_entries+0.025f)*1080)-(it*2),   1, 0, 1);
	}
/*
					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					set_texture( buffer, 1920, 1080); //PIC1.PNG
					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					flip();
*/
	}

		if(th_drive_icon==1)
		{
			if(strstr(menu[selected].path, "/dev_usb")!=NULL || strstr(menu[selected].path, "/pvd_usb")!=NULL)
			{
				put_texture( buffer, text_USB, 96, 96, 320, th_drive_icon_x, th_drive_icon_y, 0, 0x0080ff80);
			}
			else if(strstr(menu[selected].path, "/dev_hdd")!=NULL)
			{
				put_texture( buffer, text_HDD, 96, 96, 320, th_drive_icon_x, th_drive_icon_y, 0, 0xff800080);
			}
			else if(strstr(menu[selected].path, "/dev_bdvd")!=NULL)
			{
				put_texture( buffer, text_BLU_1, 96, 96, 320, th_drive_icon_x, th_drive_icon_y, 0, 0xff800080);
			}
		}

/*	if(_cover_mode==0)
	{
		float legend_font=0.9f;
		if(overscan>0.05) legend_font=(float)(legend_font-(legend_font*overscan)); legend_font=(legend_font)*24.0f;
		color= 0xff808080;
		y=0.3825f;
		x=0.65f;
		Fonts_UnbindRenderer( cf );
		ret = Fonts_SetFontEffectSlant( cf, 0.0f );
		Fonts_BindRenderer( cf, renderer );

		sprintf(str, "[X] - Load");		Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f;
		sprintf(str, "[O] - Copy");		Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f;
		sprintf(str, "[TR] - Exit");	Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f;
		sprintf(str, "[SQ] - Erase");	Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f;
		y+=0.035f;
		sprintf(str, "[L1] - Display mode");	Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f;
		sprintf(str, "[L2] - Show options");			Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f;
		sprintf(str, "[L3] - Show paths");		Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f;
		y+=0.035f;
		if(c_firmware==3.41f)
			{ sprintf(str, "[R1] - USB patch mode");	Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f; }

		if(c_firmware==3.55f)
			{ sprintf(str, "[R1] - Reset permissions");	Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f; }

		sprintf(str, "[R2] - Test source");		Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f;
		sprintf(str, "[R3] - Update");			Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, legend_font*1.2f, legend_font*1.3f, slant, step, color ); y+=0.035f;

//		ret = Fonts_SetFontEffectWeight( cf, 1.0f );
//		ret = Fonts_SetFontScale( cf, 12.0f );

//		sprintf(str, "ABCDEFGH IJKLMNOPQRSTUVWXYZ abcdef xyz 0123 798");			Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, 12.0f, 12.0f, 0.0f, step, color ); y+=0.035f;
//		ret = Fonts_SetFontScale( cf, 14.0f );
//		sprintf(str, "ABCDEFGH IJKLMNOPQRSTUVWXYZ abcdef xyz 0123 798");			Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, 14.0f, 14.0f, 0.0f, step, color ); y+=0.035f;
//		ret = Fonts_SetFontScale( cf, 16.0f );
//		sprintf(str, "ABCDEFGH IJKLMNOPQRSTUVWXYZ abcdef xyz 0123 798");			Fonts_RenderPropText( cf, surf, (int)((0.08f+x+(overscan/4.0f))*1920), (int)((y-_overscan)*1080), (uint8_t*) str, 16.0f, 16.0f, 0.0f, step, color ); y+=0.035f;
	} */


// 		draw_text_stroke( 0.08f+(overscan/4.0f), 0.81f-overscan,        legend_font, 0xc0c0c0c0, "X - Load    [] - Delete  [L3] - Paths   [R1] - Patch mode   [L1] - Cover\nO - Backup  /\\ - Exit    [R3] - Update  [R2] - Test source  [L2] - Setup");


	
			Fonts_UnbindRenderer( cf );
			Fonts_DetachFont( cf );
		}


}




//FONTS

void put_label(uint8_t *buffer, uint32_t width, uint32_t height, char *str1p, char *str2p, char *str3p, uint32_t color) //uint8_t *texture, 
{
		if(game_details==3) return;
		CellFontRenderer* renderer;
		CellFontRenderSurface* surf;
		CellFont Font[1];
		CellFont* cf;
		int fn;
		
		surf     = &RenderWork.Surface;
		cellFontRenderSurfaceInit( surf, 
		                           buffer, width*4, 4,
		                           width, height );

		cellFontRenderSurfaceSetScissor( surf, 0, 0, width, height );
		renderer = &RenderWork.Renderer;
		fn = FONT_SYSTEM_GOTHIC_JP;
		if(user_font<2 || user_font>9) fn = FONT_SYSTEM_GOTHIC_JP;
		else if (user_font==2) fn = FONT_SYSTEM_GOTHIC_LATIN;
		else if (user_font==3) fn = FONT_SYSTEM_SANS_SERIF;
		else if (user_font==4) fn = FONT_SYSTEM_SERIF;
		else if (user_font>4 && user_font<10) fn=user_font+5;
		
		int ret = Fonts_AttachFont( fonts, fn, &Font[0] );
		if ( ret == CELL_OK ) cf = &Font[0];
		else                  cf = (CellFont*)0;

		if ( cf ) {
			static float textScale = 1.00f;
			static float weight = 1.04f;
			static float slant = 0.00f;
			float surfW = (float)width;
			float surfH = (float)height;
			float textX, textW = surfW;
			float textY, textH = surfW;
			float scale;
			float step;
			float lineH, baseY;


//			char c_ver[64]; //sprintf(c_ver, "ver. %s", current_version);// (%02d/%02d/%04d %02d:%02d), timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
			uint8_t* utf8Str0 = (uint8_t*) str1p;
			uint8_t* utf8Str1 = (uint8_t*) str2p;
			uint8_t* utf8Str2 = (uint8_t*) str3p;
			float x, y, x2, x3;
			float w1,w2,w3;
			float w;
			if(str1p[0]=='_') utf8Str0++;

			step  =  0.f;
			scale=28.f;

			textX = 0.5f * ( surfW - surfW * textScale );
			textY = 0.03f * ( surfH - surfH * textScale );
			textW = surfW * textScale;
			textH = surfH * textScale;

			ret = Fonts_SetFontScale( cf, scale );
			if ( ret == CELL_OK ) {
				ret = Fonts_SetFontEffectWeight( cf, weight );
			}
			if ( ret == CELL_OK ) {
				ret = Fonts_SetFontEffectSlant( cf, slant );
			}

				ret = Fonts_GetFontHorizontalLayout( cf, &lineH, &baseY );
				
				if ( ret == CELL_OK ) {

					w1 = Fonts_GetPropTextWidth( cf, utf8Str0, scale, scale, slant, step, NULL, NULL )*1.2f;
					w2 = Fonts_GetPropTextWidth( cf, utf8Str1, scale, scale, slant, step, NULL, NULL )*0.8f;
					w3 = Fonts_GetPropTextWidth( cf, utf8Str2, scale, scale, slant, step, NULL, NULL )*0.8f;
					w = (( w1 > w2 )? w1:w2);

//	draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
//					textW-=1.0f;
					if ( w > textW ) {
						float ratio;

						scale = Fonts_GetPropTextWidthRescale( scale, w, textW, &ratio );
						w1    *= ratio;
						w2    *= ratio;
						w3    *= ratio;
						baseY *= ratio;
						lineH *= ratio;
						step  *= ratio;
					} 
				
					Fonts_BindRenderer( cf, renderer );
			
					x = 0.5f*(textW-w1);//+mouseXP; //textX-0.7f;//
					x2 = 0.5f*(textW-w2);//+mouseXP; //textX-0.7f;//
					x3 = 0.5f*(textW-w3);//+mouseXP; //textX-0.7f;//
					y=legend_y;//+mouseYP;


					if(game_details!=3)
					{
						if(game_details==0) y+=25;
						if(game_details==1) y+=12;
						Fonts_RenderPropText( cf, surf, x+1, y+1, utf8Str0, scale*1.2, scale, slant, step, 0xf0101010 );//f01010e0 //0xff404040
						blur_texture(buffer, 1920, (int)(lineH+5), (int)x-2, (int)y-2,  (int)(w1+15), (int)lineH+5, 0, 0, 3, 1);
						Fonts_RenderPropText( cf, surf, x, y, utf8Str0, scale*1.2f, scale, slant, step, color );//(color & 0x00ffffff)

						if(game_details>0)
						{
							Fonts_RenderPropText( cf, surf, x3+1, y+42, utf8Str2, scale*0.8f, scale*0.57f, slant, step, 0xff101010);
							blur_texture(buffer, 1920, (int)(lineH*0.8+5), (int)x3-2, (int)y+40,  (int)w3+15, (int)(lineH*0.57+5), 0, 0, 1, 1);
							Fonts_RenderPropText( cf, surf, x3, y+41, utf8Str2, scale*0.8f, scale*0.57f, slant, step, 0xffd0d0ff );
						}

						if(game_details>1)
						{
							Fonts_RenderPropText( cf, surf, x2+1, y+68, utf8Str1, scale*0.8f, scale*0.8f, slant, step, 0xff101010);
							blur_texture(buffer, 1920, (int)(lineH*0.8+5), (int)x2-2, (int)y+65,  (int)w2+15, (int)(lineH*0.8+5), 0, 0, 1, 1);
							Fonts_RenderPropText( cf, surf, x2, y+67, utf8Str1, scale*0.8f, scale*0.8f, slant, step, 0xc000ffff );
						}
					}

			
					Fonts_UnbindRenderer( cf );
				}

			Fonts_DetachFont( cf );
		}

}

//cellDbgFontPrintf( 0.3f, 0.45f, 0.8f, 0xc0c0c0c0, string2);
void print_label(float x, float y, float scale, uint32_t color, char *str1p, float weight, float slant, int ufont)
{
	if(max_ttf_label<512)
	{
		ttf_label[max_ttf_label].x = x;
		ttf_label[max_ttf_label].y = y;
		ttf_label[max_ttf_label].scale = scale;
		ttf_label[max_ttf_label].color = color;
		ttf_label[max_ttf_label].weight = weight;
		ttf_label[max_ttf_label].slant = slant;
		ttf_label[max_ttf_label].font = ufont;
		ttf_label[max_ttf_label].hscale = 1.0f;
		ttf_label[max_ttf_label].vscale = 1.0f;
		ttf_label[max_ttf_label].centered = 0;
		ttf_label[max_ttf_label].cut = 0.0f;

		sprintf(ttf_label[max_ttf_label].label, "%s", str1p);
		max_ttf_label++;
	}
}

void print_label_width(float x, float y, float scale, uint32_t color, char *str1p, float weight, float slant, int ufont, float cut)
{
	if(max_ttf_label<512)
	{
		ttf_label[max_ttf_label].x = x;
		ttf_label[max_ttf_label].y = y;
		ttf_label[max_ttf_label].scale = scale;
		ttf_label[max_ttf_label].color = color;
		ttf_label[max_ttf_label].weight = weight;
		ttf_label[max_ttf_label].slant = slant;
		ttf_label[max_ttf_label].font = ufont;
		ttf_label[max_ttf_label].hscale = 1.0f;
		ttf_label[max_ttf_label].vscale = 1.0f;
		ttf_label[max_ttf_label].centered = 0;
		ttf_label[max_ttf_label].cut = cut;

		sprintf(ttf_label[max_ttf_label].label, "%s", str1p);
		max_ttf_label++;
	}
}

void print_label_ex(float x, float y, float scale, uint32_t color, char *str1p, float weight, float slant, int ufont, float hscale, float vscale, int centered)
{
	if(max_ttf_label<512)
	{
		ttf_label[max_ttf_label].x = x;
		ttf_label[max_ttf_label].y = y;
		ttf_label[max_ttf_label].scale = scale;
		ttf_label[max_ttf_label].color = color;
		ttf_label[max_ttf_label].weight = weight;
		ttf_label[max_ttf_label].slant = slant;
		ttf_label[max_ttf_label].font = ufont;
		ttf_label[max_ttf_label].hscale = hscale;
		ttf_label[max_ttf_label].vscale = vscale;
		ttf_label[max_ttf_label].centered = centered;
		ttf_label[max_ttf_label].cut = 0.0f;
		sprintf(ttf_label[max_ttf_label].label, "%s", str1p);
		max_ttf_label++;
	}
}

void flush_ttf(uint8_t *buffer, uint32_t _V_WIDTH, uint32_t _V_HEIGHT)
{
	if(!max_ttf_label) return;
		uint32_t color;
		CellFontRenderer* renderer;
		CellFontRenderSurface* surf;
		CellFont Font[1];
		CellFont* cf;
		int fn;

//		uint8_t *buffer = NULL;
//		buffer=(uint8_t*)(color_base_addr)+video_buffer*(c_frame_index);
		
		surf     = &RenderWork.Surface;
		cellFontRenderSurfaceInit( surf, 
		                           buffer, _V_WIDTH*4, 4,
		                           _V_WIDTH, _V_HEIGHT );

		cellFontRenderSurfaceSetScissor( surf, 0, 0, _V_WIDTH, _V_HEIGHT );
		renderer = &RenderWork.Renderer;
		fn = FONT_SYSTEM_GOTHIC_JP;

		if(user_font<2 || user_font>9) fn = FONT_SYSTEM_GOTHIC_JP;
		else if (user_font==2) fn = FONT_SYSTEM_GOTHIC_LATIN;
		else if (user_font==3) fn = FONT_SYSTEM_SANS_SERIF;
		else if (user_font==4) fn = FONT_SYSTEM_SERIF;
		else if (user_font>4 && user_font<10) fn=user_font+5;
	
		int ret;

		if(ttf_label[0].font!=0) fn=ttf_label[0].font;
		ret	= Fonts_AttachFont( fonts, fn, &Font[0] );

		if ( ret == CELL_OK ) cf = &Font[0];
		else                  cf = (CellFont*)0;

		if ( cf ) {
			static float textScale = 1.00f;
			static float weight = 1.00f;
			static float slant = 0.00f;
			float surfW = (float)_V_WIDTH;
			float surfH = (float)_V_HEIGHT;
			float textW;// = surfW;
			float textH;// = surfH;
			float step = 0.f;
			float lineH, baseY;
			float w;
			textW = surfW * textScale;
			textH = surfH * textScale;
			uint8_t* utf8Str0;
			float scale, scaley, x, y;
			int cl=0;

			for(cl=0; cl<max_ttf_label; cl++)
			{
				weight = ttf_label[cl].weight;
				scale = 30.0f * ttf_label[cl].scale * ((float)_V_WIDTH/1920.0f) * ttf_label[cl].hscale;
				scaley = 29.0f * ttf_label[cl].scale * ((float)_V_HEIGHT/1080.0f) * ttf_label[cl].vscale;
				slant = ttf_label[cl].slant;
				ret = Fonts_SetFontScale( cf, scale );
				if ( ret == CELL_OK ) ret = Fonts_SetFontEffectWeight( cf, weight );
				if ( ret == CELL_OK ) ret = Fonts_SetFontEffectSlant( cf, slant );
				ret = Fonts_GetFontHorizontalLayout( cf, &lineH, &baseY );
				utf8Str0 = (uint8_t*) ttf_label[cl].label;
				x = ttf_label[cl].x;
				y = ttf_label[cl].y;
				color = ttf_label[cl].color; //0x80ffffff;//
				
				if ( ret == CELL_OK ) {

					w = Fonts_GetPropTextWidth( cf, utf8Str0, scale, scaley, slant, step, NULL, NULL );

					if ( w > textW && ttf_label[cl].cut==0.0f ) {
						float ratio;

						scale = Fonts_GetPropTextWidthRescale( scale, w, textW-8.f, &ratio );
						w     *= ratio;
						baseY *= ratio;
						lineH *= ratio;
						step  *= ratio;
					} 
					else if ( ttf_label[cl].cut>0.0f && w>((int)(ttf_label[cl].cut*(float)V_WIDTH))) {
						float ratio;

						scale = Fonts_GetPropTextWidthRescale( scale, w, ((int)(ttf_label[cl].cut*(float)V_WIDTH)), &ratio );
						w     *= ratio;
						baseY *= ratio;
						lineH *= ratio;
						step  *= ratio;
					} 
					
					if(ttf_label[cl].centered==1) x=(ttf_label[cl].x - (w/2.0f)/(float)_V_WIDTH);
					else if(ttf_label[cl].centered==2) x=(ttf_label[cl].x - (w)/(float)_V_WIDTH); //right justified
				

					if(cl==0) Fonts_BindRenderer( cf, renderer );			
					if(cover_mode!=8 && cover_mode!=5) Fonts_RenderPropText( cf, surf, (int)(x*(float)_V_WIDTH)+1, (int)(y *(float)_V_HEIGHT)+1, utf8Str0, scale, scaley, slant, step, 0xff000000 );
					if(cover_mode==8) Fonts_RenderPropText( cf, surf, (int)(x*(float)_V_WIDTH)+2, (int)(y *(float)_V_HEIGHT)+2, utf8Str0, scale, scaley, slant, step, 0x10101010 );
					// && cover_mode!=8
					Fonts_RenderPropText( cf, surf, (int)(x*(float)_V_WIDTH), (int)(y*(float)_V_HEIGHT), utf8Str0, scale, scaley, slant, step, color );//(color & 0x00ffffff)

				}
			}
					Fonts_UnbindRenderer( cf );

			Fonts_DetachFont( cf );
		}

	max_ttf_label=0;
}

void draw_boot_flags(u32 gflags, bool is_locked, int selected)
{
		//boot flags
		if(gflags & IS_DBOOT)
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2s_x	*4 + dox_rb2s_y	* dox_width*4), dox_rb2s_w,	dox_rb2s_h, dox_width, 580, 695, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1s_w,	dox_rb1s_h, dox_width, 580, 695, 0, 0);
		}
		else
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2u_x	*4 + dox_rb2u_y	* dox_width*4), dox_rb2u_w,	dox_rb2u_h, dox_width, 580, 695, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 580, 695, 0, 0);
		}

		if(gflags & IS_BDMIRROR)
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2s_x	*4 + dox_rb2s_y	* dox_width*4), dox_rb1u_w,	dox_rb2s_h, dox_width, 580, 735, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1u_w,	dox_rb1s_h, dox_width, 580, 735, 0, 0);
		}
		else
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2u_x	*4 + dox_rb2u_y	* dox_width*4), dox_rb2u_w,	dox_rb2u_h, dox_width, 580, 735, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 580, 735, 0, 0);
		}

		if(gflags & IS_PATCHED)
		{
			if(is_locked || c_firmware!=3.41f)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2s_x	*4 + dox_rb2s_y	* dox_width*4), dox_rb1u_w,	dox_rb2s_h, dox_width, 580, 775, 0, 0);
			else								put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1u_w,	dox_rb1s_h, dox_width, 580, 775, 0, 0);
		}
		else
		{
			if(is_locked || c_firmware!=3.41f)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2u_x	*4 + dox_rb2u_y	* dox_width*4), dox_rb2u_w,	dox_rb2u_h, dox_width, 580, 775, 0, 0);
			else								put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 580, 775, 0, 0);
		}

		if(gflags & IS_EXTGD)
		{
			if(is_locked || payload!=1)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2s_x	*4 + dox_rb2s_y	* dox_width*4), dox_rb1u_w,	dox_rb2s_h, dox_width, 580, 815, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1u_w,	dox_rb1s_h, dox_width, 580, 815, 0, 0);
		}
		else
		{
			if(is_locked || payload!=1)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2u_x	*4 + dox_rb2u_y	* dox_width*4), dox_rb2u_w,	dox_rb2u_h, dox_width, 580, 815, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 580, 815, 0, 0);
		}

		if(gflags & IS_FAV)
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2s_x	*4 + dox_rb2s_y	* dox_width*4), dox_rb1u_w,	dox_rb2s_h, dox_width, 580, 855, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1u_w,	dox_rb1s_h, dox_width, 580, 855, 0, 0);
		}
		else
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2u_x	*4 + dox_rb2u_y	* dox_width*4), dox_rb2u_w,	dox_rb2u_h, dox_width, 580, 855, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 580, 855, 0, 0);
		}

		if(selected) 
		{
//			if(selected<4)
				put_texture_with_alpha( text_FONT, text_DOX+(dox_rb3s_x	*4 + dox_rb3s_y	* dox_width*4), dox_rb3s_w,	dox_rb3s_h, dox_width, 580, 695+((selected-1)*40), 0, 0);
//			else
//				put_texture_with_alpha( text_FONT, text_DOX+(dox_rb3s_x	*4 + dox_rb3s_y	* dox_width*4), dox_rb3s_w,	dox_rb3s_h, dox_width, 580, 695+((selected)*40), 0, 0);
		}

}

void draw_reqd_flags(u32 gflags, bool is_locked, int selected)
{
		//required flags
		if(gflags & IS_DISC)
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2s_x	*4 + dox_rb2s_y	* dox_width*4), dox_rb2s_w,	dox_rb2s_h, dox_width, 240, 695, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1s_w,	dox_rb1s_h, dox_width, 240, 695, 0, 0);
		}
		else
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2u_x	*4 + dox_rb2u_y	* dox_width*4), dox_rb2u_w,	dox_rb2u_h, dox_width, 240, 695, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 240, 695, 0, 0);
		}

		if(gflags & IS_HDD)
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2s_x	*4 + dox_rb2s_y	* dox_width*4), dox_rb1u_w,	dox_rb2s_h, dox_width, 240, 735, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1u_w,	dox_rb1s_h, dox_width, 240, 735, 0, 0);
		}
		else
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2u_x	*4 + dox_rb2u_y	* dox_width*4), dox_rb2u_w,	dox_rb2u_h, dox_width, 240, 735, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 240, 735, 0, 0);
		}

		if(gflags & IS_USB)
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2s_x	*4 + dox_rb2s_y	* dox_width*4), dox_rb1u_w,	dox_rb2s_h, dox_width, 240, 775, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1u_w,	dox_rb1s_h, dox_width, 240, 775, 0, 0);
		}
		else
		{
			if(is_locked)	put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2u_x	*4 + dox_rb2u_y	* dox_width*4), dox_rb2u_w,	dox_rb2u_h, dox_width, 240, 775, 0, 0);
			else			put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 240, 775, 0, 0);
		}
		if(selected) put_texture_with_alpha( text_FONT, text_DOX+(dox_rb3s_x	*4 + dox_rb3s_y	* dox_width*4), dox_rb3s_w,	dox_rb3s_h, dox_width, 240, 695+((selected-1)*40), 0, 0);
}

//void print_label_ex(float x, float y, float scale, uint32_t color, char *str1p, 
//					float weight, float slant, int font, 
//					float hscale, float vscale, int centered);

int open_submenu(uint8_t *buffer, int *_game_sel) 
{
	xmb_bg_show=0;
	xmb_bg_counter=200;
	memcpy(text_FONT, buffer, 1920*1080*4);

reload_submenu:
	char label[256];
	float x, y, top_o;
	int m;

	get_game_flags(*_game_sel);
	if(menu_list[*_game_sel].user & IS_BDMIRROR) {menu_list[*_game_sel].user &= ~(IS_HDD | IS_DBOOT); menu_list[*_game_sel].user|= IS_USB;}
	u32 gflags=menu_list[*_game_sel].user;
	set_game_flags(*_game_sel);

	u32 oflags=0;
	bool is_locked = (gflags & IS_LOCKED) || (gflags & IS_PROTECTED) || (strstr(menu_list[*_game_sel].path,"/pvd_usb")!=NULL || strstr(menu_list[*_game_sel].path,"/dev_bdvd")!=NULL);
    bool is_game = (strstr(menu_list[*_game_sel].content,"PS3")!=NULL); //(gflags & IS_PS3) || 

	top_o=40.0f;

//	sub_menu_open=1;

	//blur_texture(text_FONT, 1920, 1080, 46, 52, 1828, 976,  35, 0, 1, 2);
	blur_texture(text_FONT, 1920, 1080, 0, 0, 1920, 1080,  25, 0, 1, 1);
//	sprintf(label, "%s/GLO.PNG", app_usrdir);
//	load_texture(text_bmp, label, 1920);
//	put_texture_with_alpha( text_FONT, text_bmp, 1920, 1080, 1920, 0, 0, 0, 0);

	/*for(m=100; m<200; m+=10)
	{
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		set_texture( buffer, 1920, 1080);  display_img((1920-1920*m/100)/2, (1080-1080*m/100)/2, 1920*m/100, 1080*m/100, 1920, 1080, -0.5f, 1920, 1080);
		setRenderColor();
		flip();
	}*/

	draw_box( text_FONT, 1920, 2, 0, (int)top_o+75, 0xa0a0a0ff);
	draw_box( text_FONT, 1920, 2, 0, 964, 0x808080ff);

	sprintf(label, "%s/%s_640.RAW", cache_dir, menu_list[*_game_sel].title_id);
	if(stat(label, &s3)>=0){
		load_texture(text_bmp, label, 640);
		if(menu_list[*_game_sel].split || menu_list[*_game_sel].title[0]=='_') 
		{ menu_list[*_game_sel].split=1; gray_texture(text_bmp, 640, 360, 0); }
		put_texture(text_FONT, text_bmp, 640, 360, 640, 105, 110+(int)top_o, 2, 0xc0c0c080);
	}

	sprintf(label, "%s/%s.JPG", covers_dir, menu_list[*_game_sel].title_id);
	if(stat(label, &s3)<0) sprintf(label, "%s/%s.PNG", covers_dir, menu_list[*_game_sel].title_id); else goto gs_cover;
	if(stat(label, &s3)<0) sprintf(label, "%s/COVER.JPG", menu_list[*_game_sel].path); else goto gs_cover;
	if(stat(label, &s3)<0) sprintf(label, "%s/COVER.PNG", menu_list[*_game_sel].path); else goto gs_cover;
	if(stat(label, &s3)<0) sprintf(label, "%s/NOID.JPG", app_usrdir); else goto gs_cover;
	if(stat(label, &s3)>=0){
gs_cover:
		load_texture(text_bmp, label, 260);
		if(menu_list[*_game_sel].split || menu_list[*_game_sel].title[0]=='_') 
		{ menu_list[*_game_sel].split=1; gray_texture(text_bmp, 260, 300, 0); }

		put_texture(text_FONT, text_bmp, 260, 300, 260, 295, 566+(int)top_o, 2, 0xc0c0c080);

		sprintf(label, "%s/GLC.PNG", app_usrdir);
		if(stat(label, &s3)>=0)
		{
			load_texture(text_bmp+312000, label, 260);
			put_texture_with_alpha( text_FONT, text_bmp+312000, 260, 300, 260, 295, 566+(int)top_o, 0, 0);
		}
	}

	put_texture_with_alpha( text_FONT, text_DOX+(dox_pad_x		*4 + dox_pad_y		* dox_width*4), dox_pad_w,		dox_pad_h,		dox_width,   70, 968, 0, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_cross_x	*4 + dox_cross_y	* dox_width*4), dox_cross_w,	dox_cross_h,	dox_width, 1140, 975, 0, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_square_x	*4 + dox_square_y	* dox_width*4), dox_square_w,	dox_square_h,	dox_width, 1340, 975, 0, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_circle_x	*4 + dox_circle_y	* dox_width*4), dox_circle_w,	dox_circle_h,	dox_width, 1540, 975, 0, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_triangle_x*4 + dox_triangle_y	* dox_width*4), dox_triangle_w,	dox_triangle_h, dox_width, 1700, 975, 0, 0);

	float game_app_ver=0.00f;
	float ps3_sys_ver=0.00f;
	char temp_val[32]; temp_val[0]=0;

	if(is_game)
	{

		draw_boot_flags(gflags, is_locked, 0);
		draw_reqd_flags(gflags, is_locked, 0);
		sprintf(label, "/dev_hdd0/game/%s/PARAM.SFO", menu_list[*_game_sel].title_id);
		if(stat(label, &s3)<0) sprintf(label, "%s/PS3_GAME/PARAM.SFO", menu_list[*_game_sel].path);
		if(stat(label, &s3)<0) sprintf(label, "%s/PARAM.SFO", menu_list[*_game_sel].path);
		if(get_param_sfo_field(label, (char *)"APP_VER", (char *)temp_val)) game_app_ver=strtof(temp_val, NULL);
		else if(get_param_sfo_field(label, (char *)"VERSION", (char *)temp_val)) game_app_ver=strtof(temp_val, NULL);
		if(get_param_sfo_field(label, (char *)"PS3_SYSTEM_VER", (char *)temp_val)) ps3_sys_ver=strtof(temp_val, NULL);
		temp_val[0]=0;
		if(game_app_ver && ps3_sys_ver)
			sprintf(temp_val, " ver. %4.2f (PS3 firmware %4.2f)", game_app_ver, ps3_sys_ver);
		else if(game_app_ver && !ps3_sys_ver)
			sprintf(temp_val, " ver. %4.2f", game_app_ver);
		else if(ps3_sys_ver) 
			sprintf(temp_val, " (PS3 firmware %4.2f)", ps3_sys_ver);

//		put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1s_w,	dox_rb1s_h, dox_width, 580, 695, 0, 0);
//		put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 580, 735, 0, 0);
//		put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2s_x	*4 + dox_rb2s_y	* dox_width*4), dox_rb2s_w,	dox_rb2s_h, dox_width, 580, 775, 0, 0);

//		put_texture_with_alpha( text_FONT, text_DOX+(dox_rb2u_x	*4 + dox_rb2u_y	* dox_width*4), dox_rb2u_w,	dox_rb2u_h, dox_width, 240, 695, 0, 0);
//		put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1s_x	*4 + dox_rb1s_y	* dox_width*4), dox_rb1s_w,	dox_rb1s_h, dox_width, 240, 735, 0, 0);
//		put_texture_with_alpha( text_FONT, text_DOX+(dox_rb1u_x	*4 + dox_rb1u_y	* dox_width*4), dox_rb1u_w,	dox_rb1u_h, dox_width, 240, 775, 0, 0);
	}

//	put_texture_with_alpha( text_FONT, text_DOX+(dox_att_x	*4 + dox_att_y	* dox_width*4), dox_att_w,	dox_att_h, dox_width, 950, 1000, 0, 0);

	max_ttf_label=0;
	char *game_title = menu_list[*_game_sel].title[0]=='_' ? menu_list[*_game_sel].title+1 : menu_list[*_game_sel].title;
	x=860.0f;
	y=top_o+155.0f;
	u32 title_color=0xffc0c0c0;

	int x_icon=(int)(x-50);
	int y_icon=(int) (top_o + 158.0f);
	int option_number=0; // + (120.f * (option_number-1))

	sprintf(label, "Backup / Copy");
	if(disable_options==2 || disable_options==3) {title_color=0xd0808080;} else {title_color=0xffc0c0c0; put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0); oflags|=(1<<0); }
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_w_x*4 + dox_arrow_w_y	* dox_width*4), dox_arrow_w_w,	dox_arrow_w_h, dox_width, (int)x-50, (int)y+3, 0, 0);

	y+=120.0f;
	if(is_locked || disable_options==1 || disable_options==3) title_color=0xd0808080; else {title_color=0xffc0c0c0; put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0); oflags|=(1<<1);}
	sprintf(label, "Delete");
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

	y+=120.0f;
	if(is_locked || !is_game) title_color=0xd0808080; else {title_color=0xffc0c0c0; put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0);oflags|=(1<<2);}
	sprintf(label, "Rename");
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

	y+=120.0f;
	if(!is_game) title_color=0xd0808080; else {title_color=0xffc0c0c0; put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0);oflags|=(1<<3);}
	sprintf(label, "Update");
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

	title_color=0xffc0c0c0;
	y+=120.0f;
	sprintf(label, "Test"); oflags|=(1<<4);
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0);

//	title_color=0xd0808080;
	y+=120.0f;
	if(is_locked) title_color=0xd0808080; else {title_color=0xffc0c0c0; put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0);oflags|=(1<<5);}
	sprintf(label, "Permissions");
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

//	y+=120.0f;
//	sprintf(label, "Add to favorites");
//	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

	u32 info_color=0xffa0a0a0;
	u32 dev_color=0xffe0e0e0;
	if(is_game)
	{
		if(is_locked) dev_color=0xc0808080;
		print_label_ex( (225.f/1920.f), ((top_o+657.f)/1080.f), 1.5f, dev_color, (char*)"Game disc", 1.00f, 0.00f, 15, 0.4f, 0.5f, 2);
		print_label_ex( (225.f/1920.f), ((top_o+697.f)/1080.f), 1.5f, dev_color, (char*)"Internal", 1.00f, 0.00f, 15, 0.4f, 0.5f, 2);
		print_label_ex( (225.f/1920.f), ((top_o+737.f)/1080.f), 1.5f, dev_color, (char*)"External", 1.00f, 0.00f, 15, 0.4f, 0.5f, 2);

		print_label_ex( (620.f/1920.f), ((top_o+660.f)/1080.f), 1.5f, dev_color, (char*)"Direct boot", 1.00f, 0.00f, 15, 0.4f, 0.5f, 0);
		print_label_ex( (620.f/1920.f), ((top_o+700.f)/1080.f), 1.5f, dev_color, (char*)"BD mirror", 1.00f, 0.00f, 15, 0.4f, 0.5f, 0);
		if(payload!=1) dev_color=0xc0808080;
		print_label_ex( (620.f/1920.f), ((top_o+780.f)/1080.f), 1.5f, dev_color, (char*)"Ext Game Data", 1.00f, 0.00f, 15, 0.4f, 0.5f, 0);dev_color=0xffe0e0e0;
		print_label_ex( (620.f/1920.f), ((top_o+820.f)/1080.f), 1.5f, dev_color, (char*)"Favorite", 1.00f, 0.00f, 15, 0.4f, 0.5f, 0);
		if(c_firmware!=3.41) dev_color=0xc0808080;
		print_label_ex( (620.f/1920.f), ((top_o+740.f)/1080.f), 1.5f, dev_color, (char*)"USB patch", 1.00f, 0.00f, 15, 0.4f, 0.5f, 0);
	}

	time ( &rawtime ); timeinfo = localtime ( &rawtime );
	
	if(date_format==0)	sprintf(label,"%d/%d %s:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, tmhour(timeinfo->tm_hour), timeinfo->tm_min);
	else sprintf(label,"%d/%d %s:%02d", timeinfo->tm_mon+1, timeinfo->tm_mday, tmhour(timeinfo->tm_hour), timeinfo->tm_min);
	print_label_ex( (1690.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);
	sprintf(label, "%s%s", game_title, temp_val);
	print_label_ex( (70.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	if(strstr(menu_list[*_game_sel].path, "/dev_bdvd")!=NULL) dev_color=0xffe0e0e0; else dev_color=0xc0808080;
	print_label_ex( (1460.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, dev_color, (char*)"Blu", 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	if(strstr(menu_list[*_game_sel].path, "/dev_hdd")!=NULL) dev_color=0xffe0e0e0; else dev_color=0xc0808080;
	print_label_ex( (1530.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, dev_color, (char*)"Hdd", 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	if(strstr(menu_list[*_game_sel].path, "/dev_usb")!=NULL || strstr(menu_list[*_game_sel].path, "/pvd_usb")!=NULL) dev_color=0xffe0e0e0; else dev_color=0xc0808080;
	print_label_ex( (1600.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, dev_color, (char*)"Usb", 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	sprintf(label, "%s", " Navigate");
	print_label_ex( ((70.f+dox_pad_w)/1920.f), (981.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	sprintf(label, "%s", " Select");
	print_label_ex( ((1140.f+dox_cross_w)/1920.f), (980.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	sprintf(label, "%s", " Genre");
	print_label_ex( ((1340.f+dox_square_w)/1920.f), (980.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	sprintf(label, "%s", " Back");
	print_label_ex( ((1540.f+dox_circle_w)/1920.f), (980.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	sprintf(label, "%s", " Cancel");
	print_label_ex( ((1700.f+dox_triangle_w)/1920.f), (980.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	info_color=0xffa0a0a0;
	x+=20;

	if(disable_options==2 || disable_options==3) info_color=0xc0707070; else info_color=0xffa0a0a0;
	y=top_o+(120.0f * 1.0f) + 77.0f;
	sprintf(label, "Create a backup copy of \x22%s\x22.", game_title); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "To protect your Playstation\xC2\xAE\x33 Blu-ray\xE2\x84\xA2 game disc,"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "transfer its contents to internal or external hard disk drive."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	if(is_locked || disable_options==1 || disable_options==3) info_color=0xc0707070; else info_color=0xffa0a0a0;
	y=top_o+(120.0f * 2.0f) + 77.0f;
	sprintf(label, "Permanently delete game files."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "If you are running low on disk space, delete the game from your"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "hard disk drive. Use this option with care!"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	if(is_locked || !is_game) info_color=0xc0707070; else info_color=0xffa0a0a0;
	y=top_o+(120.0f * 3.0f) + 77.0f;
	sprintf(label, "Pick a name of your choice for the game."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "You can use the on-screen keyboard or external USB keyboard"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "for input. Use (TM), (R) and (C) to enter \xE2\x84\xA2, \xC2\xAE and \xC2\xA9 symbols."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	if(!is_game) info_color=0xc0707070; else info_color=0xffa0a0a0;
	y=top_o+(120.0f * 4.0f) + 77.0f;
	sprintf(label, "Check for game updates."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "This feature allows you to download all available updates"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "or only the latest. Internet connection required."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	info_color=0xffa0a0a0;
	y=top_o+(120.0f * 5.0f) + 77.0f;
	sprintf(label, "Verify all files and folders of the game."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "Testing will report various data as Total Size, Number of files,"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "Big files (over 4GB) and if game contains split (.666##) files."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

//	info_color=0xc0707070;
	if(is_locked) info_color=0xc0707070; else info_color=0xffa0a0a0;
	y=top_o+(120.0f * 6.0f) + 77.0f;
	sprintf(label, "Re-apply file and folder access permissions."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "On rare occasions it may be required to perform resetting of"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "ownership and execution flags of game contents."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

//	y=top_o+(120.0f * 7.0f) + 32.0f;
//	sprintf(label, "Add \x22%s\x22 to your list of", game_title); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
//	sprintf(label, "favorite games. Use the favorites menu to quickly select"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
//	sprintf(label, "the games you play most often."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	//sprintf(label, "multiMAN %s", current_version); label[17]=0;
	//print_label_ex( (960.f/1920.f), (980.f/1080.f), 1.0f, 0x70707070, label, 1.04f, 0.00f, 1, 0.6f, 0.6f, 1);

	if((*_game_sel)) sprintf(label, "%s", "Prev Title"); else sprintf(label, "%s", "Last Title");
	print_label_ex( ((750.f)/1920.f), (981.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 2);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_l1_x*4 + dox_l1_y	* dox_width*4), dox_l1_w,	dox_l1_h, dox_width, 770, 982, 0, 0);

	if((*_game_sel)!=(max_menu_list-1)) sprintf(label, "%s", " Next Title"); else sprintf(label, "%s", " First Title");
	print_label_ex( ((840.f+dox_r1_w)/1920.f), (981.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_r1_x*4 + dox_r1_y	* dox_width*4), dox_r1_w,	dox_r1_h, dox_width, 840, 982, 0, 0);

	if(!(menu_list[*_game_sel].split || menu_list[*_game_sel].title[0]=='_' || strstr(menu_list[*_game_sel].path, "/pvd_usb")!=NULL)) 
	{
		sprintf(label, "%s", " Load");
		print_label_ex( ((375.f+dox_start_w)/1920.f), (981.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);
		put_texture_with_alpha( text_FONT, text_DOX+(dox_start_x*4 + dox_start_y	* dox_width*4), dox_start_w,	dox_start_h, dox_width, 375, 972, 0, 0);
	}
	else
		put_texture_with_alpha( text_FONT, text_DOX+(dox_att_x*4 + dox_att_y	* dox_width*4), dox_att_w,	dox_att_h, dox_width, 400, 972, 0, 0);

	sprintf(label, "Genre: %s", genre[ (menu_list[*_game_sel].user>>16)&0x0f ]);
	print_label_ex( (420.0f/1920.f), ((538.0f+top_o)/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 1);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_start_x*4 + dox_start_y	* dox_width*4), dox_start_w,	dox_start_h, dox_width, 375, 972, 0, 0);

	flush_ttf(text_FONT, 1920, 1080);

	if(menu_list[*_game_sel].split || menu_list[*_game_sel].title[0]=='_') 
	{
		menu_list[*_game_sel].split=1;
		sprintf(label, "%s (Split)", game_title);
		print_label_ex( (425.0f/1920.f), ((478.0f+top_o)/1080.f), 1.0f, COL_SPLIT, label, 1.04f, 0.0f, 1, 0.6f, 0.6f, 1);
	}
	else
	{
		sprintf(label, "%s", game_title);
		print_label_ex( (425.0f/1920.f), ((478.0f+top_o)/1080.f), 1.0f, 0xc0e0e0e0, label, 1.04f, 0.0f, 1, 0.6f, 0.6f, 1);
	}
	flush_ttf(text_FONT, 1920, 1080);

	sprintf(label, "[%s]", menu_list[*_game_sel].path+5); if(strlen(label)>64) {label[64]=0x2e;label[65]=0x2e;label[66]=0x0;}
	print_label_ex( (425.0f/1920.f), ((507.0f+top_o)/1080.f), 1.0f, 0xc0808080, label, 1.00f, 0.0f, 17, 0.7f, 0.7f, 1);
	flush_ttf(text_FONT, 1920, 1080);

	sprintf(label, "%s", menu_list[*_game_sel].title_id);
	print_label_ex( (425.0f/1920.f), ((873.0f+top_o)/1080.f), 1.0f, 0xc0c0c0c0, label, 1.00f, 0.0f, 18, 1.0f, 1.0f, 1);
	flush_ttf(text_FONT, 1920, 1080);

	//u32 b_color=0x0080ffd0;

//	memcpy(buffer, text_FONT, 1920*1080*4);

	if(V_WIDTH<1280)
	{
		//sub_menu_open=0;
		blur_texture(text_FONT, 1920, 1080, 0, 0, 1920, 1080,  0, 0, 1, 1);
		//sub_menu_open=1;
	}

	for(m=200; m>100; m-=10)
	{
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		set_texture( text_FONT, 1920, 1080);  display_img((1920-1920*m/100)/2, (1080-1080*m/100)/2, 1920*m/100, 1080*m/100, 1920, 1080, -0.5f, 1920, 1080);
		setRenderColor();
		flip();
	}

	int result=0;
	int main_options=1; //1-main 2-gameboot, 3-gamereq
	int options_req=1;
	int options_boot=1;

	u32 b_color = 0x0080ffff;
	while (1) {
		//cellSysutilCheckCallback();
		pad_read();

		if ( (new_pad & BUTTON_L1)) {
			menu_list[*_game_sel].user=gflags;
			set_game_flags(*_game_sel);
			(*_game_sel)--;
			if((*_game_sel)<0) (*_game_sel)=(max_menu_list-1);
			sprintf(label, "%s/%s_1920.PNG", cache_dir, menu_list[*_game_sel].title_id);
			if(stat(label, &s3)>=0)	load_texture(text_FONT, label, 1920); else memset(text_FONT, 0, 1920*1080*4);
			goto reload_submenu;
			} 

		if ( (new_pad & BUTTON_R1)) {
			menu_list[*_game_sel].user=gflags;
			set_game_flags(*_game_sel);
			(*_game_sel)++;
			if((*_game_sel)>=max_menu_list) (*_game_sel)=0;
			sprintf(label, "%s/%s_1920.PNG", cache_dir, menu_list[*_game_sel].title_id);
			if(stat(label, &s3)>=0)	load_texture(text_FONT, label, 1920); else memset(text_FONT, 0, 1920*1080*4);
			goto reload_submenu;
			} 


		if ( (new_pad & BUTTON_SQUARE)) 
		{
			use_analog=1;
			float b_mX=mouseX;
			float b_mY=mouseY;
			mouseX=660.f/1920.f;
			mouseY=225.f/1080.f;
			for (int n=0;n<16;n++ )
			{
				sprintf(opt_list[n].label, "%s", genre[n]);
				sprintf(opt_list[n].value, "%i", n);
			}
			opt_list_max=16;
			int ret_f=open_select_menu((char*) "Select Genre", 600, opt_list, opt_list_max, text_FONT, 16, 1);
			use_analog=0;
			mouseX=b_mX;
			mouseY=b_mY;
			if(ret_f!=-1) 
			{
				menu_list[*_game_sel].user=(gflags & (u32)(~(15<<16))) | ((u32)(strtod(opt_list[ret_f].value, NULL))<<16);
				set_game_flags(*_game_sel);
				sprintf(label, "%s/%s_1920.PNG", cache_dir, menu_list[*_game_sel].title_id);
				if(stat(label, &s3)>=0)	load_texture(text_FONT, label, 1920); else memset(text_FONT, 0, 1920*1080*4);
				goto reload_submenu;
			}
			else new_pad=0;
		} 


		if ( (new_pad & BUTTON_START)) {
			if(!(menu_list[*_game_sel].split || menu_list[*_game_sel].title[0]=='_' || strstr(menu_list[*_game_sel].path, "/pvd_usb")!=NULL)) 
			{
				menu_list[*_game_sel].user=gflags;
				set_game_flags(*_game_sel);
				result=7;
				break;
			}
		}


		if ( (new_pad & BUTTON_TRIANGLE)) {result=0; break;} //quit sub-menu
		if ( (new_pad & BUTTON_CIRCLE)) {
			if(is_locked || !is_game)
			{
				dialog_ret=0;
				cellMsgDialogOpen2( type_dialog_ok, "Options cannot be changed or title is locked!", dialog_fun2, (void*)0x0000aaab, NULL );
				wait_dialog_simple();
			}
			else
			{
				menu_list[*_game_sel].user=gflags;
				if(!set_game_flags(*_game_sel))
				{
					dialog_ret=0;
					cellMsgDialogOpen2( type_dialog_ok, "Options cannot be changed for this title!", dialog_fun2, (void*)0x0000aaab, NULL );
					wait_dialog_simple();
				}
			}
			result=0;
			break;
		} //save changes and quit sub-menu

		if ( (new_pad & BUTTON_LEFT) && is_game && !is_locked)
		{
			put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, x_icon, (int)(y_icon + (120.f * (option_number))), 0, 0);
			main_options++;
			if(main_options>3) main_options=1;
			if(main_options<1) main_options=3;
			if(main_options==1)	{draw_boot_flags(gflags, is_locked, 0); draw_reqd_flags(gflags, is_locked, 0); 				put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_w_x*4 + dox_arrow_w_y	* dox_width*4), dox_arrow_w_w,	dox_arrow_w_h, dox_width, x_icon, (int)(y_icon + (120.f * (option_number))), 0, 0);}
			if(main_options==2)	{draw_boot_flags(gflags, is_locked, options_boot); draw_reqd_flags(gflags, is_locked, 0);}
			if(main_options==3)	{draw_boot_flags(gflags, is_locked, 0); draw_reqd_flags(gflags, is_locked, options_req);}
		}

		if ( (new_pad & BUTTON_RIGHT) && is_game && !is_locked)
		{
			put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, x_icon, (int)(y_icon + (120.f * (option_number))), 0, 0);
			main_options--;
			if(main_options<1) main_options=3;
			if(main_options==1)	{draw_boot_flags(gflags, is_locked, 0); draw_reqd_flags(gflags, is_locked, 0);				put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_w_x*4 + dox_arrow_w_y	* dox_width*4), dox_arrow_w_w,	dox_arrow_w_h, dox_width, x_icon, (int)(y_icon + (120.f * (option_number))), 0, 0);}
			if(main_options==2)	{draw_boot_flags(gflags, is_locked, options_boot); draw_reqd_flags(gflags, is_locked, 0);}
			if(main_options==3)	{draw_boot_flags(gflags, is_locked, 0); draw_reqd_flags(gflags, is_locked, options_req);}
		}

		if(main_options==1)
		{
			if ( (new_pad & BUTTON_DOWN))
			{
				put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, x_icon, (int)(y_icon + (120.f * (option_number))), 0, 0);
				int oloop;
				for(oloop=option_number; oloop<7; oloop++)
				{
					option_number++;
					if(oflags & (1<<option_number)) break;
				}
				if(option_number>6) option_number=0;
				put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_w_x*4 + dox_arrow_w_y	* dox_width*4), dox_arrow_w_w,	dox_arrow_w_h, dox_width, x_icon, (int)(y_icon + (120.f * (option_number))), 0, 0);
			}

			if ( (new_pad & BUTTON_UP))
			{
				put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, x_icon, (int)(y_icon + (120.f * (option_number))), 0, 0);
				int oloop;
				if(option_number==0) option_number=7;
				for(oloop=option_number; oloop>=0; oloop--)
				{
					option_number--;
					if(oflags & (1<<option_number)) break;
				}
				if(option_number<0) option_number=0;
				put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_w_x*4 + dox_arrow_w_y	* dox_width*4), dox_arrow_w_w,	dox_arrow_w_h, dox_width, x_icon, (int)(y_icon + (120.f * (option_number))), 0, 0);
			}

			if ( (new_pad & BUTTON_CROSS) || (new_pad & BUTTON_L3) || (new_pad & BUTTON_R3)){
				result=option_number+1;
				if(!is_locked && is_game) {	menu_list[*_game_sel].user=gflags;set_game_flags(*_game_sel); }
				//save changed options on main options activation
				break;
			}
		}

		if (main_options==2){

			if (new_pad & BUTTON_DOWN)
			{
				options_boot++;
				if(options_boot>5) options_boot=1;
				draw_boot_flags(gflags, is_locked, options_boot);
			}

			if (new_pad & BUTTON_UP)
			{
				options_boot--;
				if(options_boot<1) options_boot=5;
				draw_boot_flags(gflags, is_locked, options_boot);
			}

			if ( (new_pad & BUTTON_CROSS) || (new_pad & BUTTON_L3) || (new_pad & BUTTON_R3))
			{
				if(options_boot<4)
					gflags ^= ( 1 << (options_boot+4) );
				else
				{
					if(options_boot==4) gflags ^= ( 1 << (options_boot+5) );
					if(options_boot==5) gflags ^= ( 1 << (options_boot+3) );
				}
				
				if(gflags & IS_BDMIRROR) {gflags &= ~(IS_HDD | IS_DBOOT); gflags|= IS_USB;}
				draw_boot_flags(gflags, is_locked, options_boot);
				draw_reqd_flags(gflags, is_locked, 0);
			}

		}

		if (main_options==3){
			if (new_pad & BUTTON_DOWN)
			{
				options_req++;
				if(options_req>3) options_req=1;
				draw_reqd_flags(gflags, is_locked, options_req);
			}

			if (new_pad & BUTTON_UP)
			{
				options_req--;
				if(options_req<1) options_req=3;
				draw_reqd_flags(gflags, is_locked, options_req);
			}

			if ( (new_pad & BUTTON_CROSS) || (new_pad & BUTTON_L3) || (new_pad & BUTTON_R3))
			{
				gflags ^= ( 1 << (options_req-1) );
				if(gflags & IS_BDMIRROR) {gflags &= ~(IS_HDD | IS_DBOOT); gflags|= IS_USB;}
				draw_boot_flags(gflags, is_locked, 0);
				draw_reqd_flags(gflags, is_locked, options_req);
			}
		}



		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

/*		mouseX+=mouseXD; mouseY+=mouseYD;
		if(mouseX>0.995f) {mouseX=0.995f;mouseXD=0.0f;} if(mouseX<0.0f) {mouseX=0.0f;mouseXD=0.0f;}
		if(mouseY>0.990f) {mouseY=0.990f;mouseYD=0.0f;} if(mouseY<0.0f) {mouseY=0.0f;mouseYD=0.0f;}
*/
		b_box_opaq+=b_box_step;
		if(b_box_opaq>0xfe) b_box_step=-1;
		if(b_box_opaq<0x20) b_box_step= 2;
		b_color = (b_color & 0xffffff00) | (b_box_opaq);
		//105, 120+(int)top_o,
		draw_square(((0.054f-0.5f)*2.0f)-0.005f, ((0.5f-(110.f+top_o)/1080.f)+0.005f)*2.0f , 0.675f, 0.68f, -0.4f, b_color);
		cellDbgFontPrintf( 0.99f, 0.98f, 0.5f,0x60606040, payloadT); 
		set_texture( text_FONT, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, -0.5f, 1920, 1080);

		//setRenderColor();
		//cellDbgFontDrawGcm();
//		draw_mouse_pointer(0);
		flip();

	}
	old_fi=-1; game_last_page=-1;
	new_pad=0; old_pad=0;
	//sub_menu_open=0;
	return result;
}

int open_mm_submenu(uint8_t *buffer) //, int *_game_sel
{
	xmb_bg_show=0;
	xmb_bg_counter=200;
	char label[256];
	float x, y, top_o;
	int m;

//	get_game_flags(*_game_sel);
//	if(menu_list[*_game_sel].user & IS_BDMIRROR) {menu_list[*_game_sel].user &= ~(IS_HDD | IS_DBOOT); menu_list[*_game_sel].user|= IS_USB;}
//	u32 gflags=menu_list[*_game_sel].user;
//	set_game_flags(*_game_sel);

	u32 oflags=1;
//	bool is_locked = (gflags & IS_LOCKED) || (gflags & IS_PROTECTED) || (strstr(menu_list[*_game_sel].path,"/pvd_usb")!=NULL || strstr(menu_list[*_game_sel].path,"/dev_bdvd")!=NULL);
//  bool is_game = (strstr(menu_list[*_game_sel].content,"PS3")!=NULL); //(gflags & IS_PS3) || 

	top_o=40.0f;

	//sub_menu_open=1;

	//blur_texture(text_FONT, 1920, 1080, 46, 52, 1828, 976,  35, 0, 1, 2);
	if(cover_mode!=8) 
	{
		memcpy(text_FONT, buffer, 0x7E9000);
		blur_texture(text_FONT, 1920, 1080, 0, 0, 1920, 1080,  25, 0, 1, 1);

		for(m=100; m<200; m+=10)
		{
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			set_texture( buffer, 1920, 1080);  display_img((1920-1920*m/100)/2, (1080-1080*m/100)/2, 1920*m/100, 1080*m/100, 1920, 1080, -0.5f, 1920, 1080);
			setRenderColor();
			flip();
		}
	}
	else memset(text_FONT, 0, 0x7E9000);

	draw_box( text_FONT, 1920, 2, 0, (int)top_o+75, 0xa0a0a0ff);
	draw_box( text_FONT, 1920, 2, 0, 964, 0x808080ff);

	put_texture_with_alpha( text_FONT, text_DOX+(dox_pad_x		*4 + dox_pad_y		* dox_width*4), dox_pad_w,		dox_pad_h,		dox_width,   70, 968, 0, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_cross_x	*4 + dox_cross_y	* dox_width*4), dox_cross_w,	dox_cross_h,	dox_width, 1520, 975, 0, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_circle_x	*4 + dox_circle_y	* dox_width*4), dox_circle_w,	dox_circle_h,	dox_width, 1700, 975, 0, 0);

	max_ttf_label=0;

	x=150.0f;
	y=top_o+95.0f;
	u32 title_color=0xffc0c0c0;

	sprintf(label, "Update");
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_w_x*4 + dox_arrow_w_y	* dox_width*4), dox_arrow_w_w,	dox_arrow_w_h, dox_width, (int)x-50, (int)y+3, 0, 0);

		sprintf(label, "Refresh List");
		print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0); oflags|=(1<<6);
		put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x+910, (int)y+3, 0, 0);

	y+=135.0f;
	if(lock_fileman) title_color=0xd0808080; else {title_color=0xffc0c0c0;oflags|=(1<<1);put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0); }
//	title_color=0xffc0c0c0; put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0); if(!lock_fileman) oflags|=(1<<1);
	sprintf(label, "File Manager");
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

		title_color=0xffc0c0c0;
		sprintf(label, "Launch Showtime");
		print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0); oflags|=(1<<7);
		put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x+910, (int)y+3, 0, 0);

	y+=135.0f;
	title_color=0xffc0c0c0; put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0); oflags|=(1<<2);
	sprintf(label, "PFS / NTFS driver");
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

		sprintf(label, "Link VIDEO to Showtime");
		print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0); oflags|=(1<<8);
		put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x+910, (int)y+3, 0, 0);

	y+=135.0f;
	sprintf(label, "Screenshot");
	if(cover_mode==3 or cover_mode==4) title_color=0xd0808080; else {title_color=0xffc0c0c0;oflags|=(1<<3);put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0); }
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

		title_color=0xffc0c0c0;
		sprintf(label, "Screensaver");
		print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0); oflags|=(1<<9);
		put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x+910, (int)y+3, 0, 0);	

	title_color=0xffc0c0c0;
	y+=135.0f;
	sprintf(label, "Restart"); oflags|=(1<<4);
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);
	put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0);

		sprintf(label, "multiMAN Setup");
		print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0); oflags|=(1<<10);
		put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x+910, (int)y+3, 0, 0);	

//	title_color=0xd0808080;
	y+=135.0f;
	title_color=0xffc0c0c0; put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x-50, (int)y+3, 0, 0); oflags|=(1<<5);
	sprintf(label, "Quit");
	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

		sprintf(label, "Help");
		print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0); oflags|=(1<<11);
		put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, (int)x+910, (int)y+3, 0, 0);	

//	y+=120.0f;
//	sprintf(label, "Add to favorites");
//	print_label_ex( (x/1920.f), (y/1080.f), 1.5f, title_color, label, 1.04f, 0.0f, 15, 1.0f, 1.0f, 0);

	u32 info_color=0xffa0a0a0;

	time ( &rawtime ); timeinfo = localtime ( &rawtime );
	if(date_format==0)	sprintf(label,"%d/%d %s:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, tmhour(timeinfo->tm_hour), timeinfo->tm_min);
	else sprintf(label,"%d/%d %s:%02d", timeinfo->tm_mon+1, timeinfo->tm_mday, tmhour(timeinfo->tm_hour), timeinfo->tm_min);
	print_label_ex( (1690.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	sprintf(label, "multiMAN %s", current_version); label[17]=0;	
	print_label_ex( (70.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

/*	if(strstr(menu_list[*_game_sel].path, "/dev_bdvd")!=NULL) dev_color=0xffe0e0e0; else dev_color=0xc0808080;
	print_label_ex( (1460.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, dev_color, (char*)"Blu", 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	if(strstr(menu_list[*_game_sel].path, "/dev_hdd")!=NULL) dev_color=0xffe0e0e0; else dev_color=0xc0808080;
	print_label_ex( (1530.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, dev_color, (char*)"Hdd", 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	if(strstr(menu_list[*_game_sel].path, "/dev_usb")!=NULL || strstr(menu_list[*_game_sel].path, "/pvd_usb")!=NULL) dev_color=0xffe0e0e0; else dev_color=0xc0808080;
	print_label_ex( (1600.f/1920.f), ((top_o+50.f)/1080.f), 1.5f, dev_color, (char*)"Usb", 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);
*/
	sprintf(label, "%s", " Navigate");
	print_label_ex( ((70.f+dox_pad_w)/1920.f), (981.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	sprintf(label, "%s", " Select");
	print_label_ex( ((1520.f+dox_cross_w)/1920.f), (980.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	sprintf(label, "%s", " Back");
	print_label_ex( ((1700.f+dox_circle_w)/1920.f), (980.f/1080.f), 1.5f, info_color, label, 1.00f, 0.00f, 15, 0.5f, 0.5f, 0);

	info_color=0xffa0a0a0;
	x+=20;

	info_color=0xffa0a0a0;
	y=top_o+(135.0f * 1.0f);
	sprintf(label, "Check for available program updates."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "multiMAN is being updated constantly, so make sure"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "to check for new versions regularly."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "Internet connection required."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y-=60.0f;

		sprintf(label, "Re-scan internal and external hard disk drives."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "Force multiMAN to refresh and re-detect content"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "(games, video, other). Use this option if you"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "transfer content via FTP. Shortcut is SELECT+L3."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	y=top_o+(135.0f * 2.0f);
	if(lock_fileman) info_color=0xc0707070; else info_color=0xffa0a0a0;
	sprintf(label, "Switch to File Manager mode. Shortcut is SELECT+START."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "Use file manager to manage your files and folders, to copy,"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "move or rename them, to view images and play music and video."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "SHOWTIME may be required to play some content."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y-=60.0f;

	info_color=0xffa0a0a0;
		sprintf(label, "Quit multiMAN and launch Showtime Player."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "Showtime Media Player is a feature rich"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "application, which allows you to play"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "various video, audio and graphic formats."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	y=top_o+(135.0f * 3.0f);
	sprintf(label, "Switch between FAT32 and NTFS drivers."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "To use this option you must connect external USB HDD,"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "formatted with FAT32 or NTFS file system."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "Proper USB.CFG required. Shortcut is START+TRIANGLE."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y-=60.0f;

		sprintf(label, "Create links of your XMB\xE2\x84\xA2 video files to"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "a special folder for Showtime Media Player"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "to find them. When linking completes, multiMAN"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "will launch Showtime."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	y=top_o+(135.0f * 4.0f);
	if(cover_mode==3 or cover_mode==4) info_color=0xc0707070; else info_color=0xffa0a0a0;
	sprintf(label, "Take a screenshot of your game list."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "Current screen will be saved as RGB raw file to /dev_hdd0"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "or in the root folder of connected USB device."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "Shortcut is START+R2."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y-=60.0f;

		info_color=0xffa0a0a0;
 		sprintf(label, "Turn on multiMAN's screensaver."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "A screen with falling 'stars' will appear on"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "your display. Press a button to quit"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "the screensaver mode."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	info_color=0xffa0a0a0;
	y=top_o+(135.0f * 5.0f);
	sprintf(label, "Restart multiMAN. Shortcut is START+SELECT."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "You can remotely restart multiMAN by connecting to your"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "Playstation\xC2\xAE\x33 system via telnet to port 8080"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "and type 'restart'."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y-=60.0f;

 		sprintf(label, "Switch to XMMB Settings column."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "You can edit all multiMAN options in XMMB mode."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "If you select this option, the current display"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "mode will be changed."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

//	info_color=0xc0707070;
	y=top_o+(135.0f * 6.0f);
	sprintf(label, "Quit multiMAN and return to XMB\xE2\x84\xA2 home screen."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "You can remotely quit multiMAN by connecting to your"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "Playstation\xC2\xAE\x33 system via telnet to port 8080"); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
	sprintf(label, "and type 'quit'."); print_label_ex( (x/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y-=60.0f;

 		sprintf(label, "Start the HELP application."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "This option will execute external"); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "help.MME application and quit multiMAN."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;
		sprintf(label, "Avoid using it during FTP transfers."); print_label_ex( ((x+960)/1920.f), (y/1080.f), 1.5f, info_color, label, 1.00f, 0.05f, 15, 0.5f, 0.5f, 0); y+=20.0f;

	flush_ttf(text_FONT, 1920, 1080);

//	memcpy(buffer, text_FONT, 1920*1080*4);

	if(V_WIDTH<1280) blur_texture(text_FONT, 1920, 1080, 0, 0, 1920, 1080,  0, 0, 1, 1);


	for(m=200; m>100; m-=10)
	{
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		set_texture( text_FONT, 1920, 1080);  display_img((1920-1920*m/100)/2, (1080-1080*m/100)/2, 1920*m/100, 1080*m/100, 1920, 1080, -0.5f, 1920, 1080);
		setRenderColor();
		flip();
	}

	int result=0;
	int main_options=1; //1-main 2-gameboot, 3-gamereq
	int x_icon=(int)(100);
	int y_icon=(int) (top_o + 98.0f);
	int option_number=0;
	int c_option_number=0;

//	int options_req=1;
//	int options_boot=1;

//	u32 b_color = 0x0080ffff;
	while (1) {
		//cellSysutilCheckCallback();
		pad_read();

		if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) ) {result=0; break;} //quit sub-menu

		if(main_options==1)
		{
			if ( (new_pad & BUTTON_UP) || (new_pad & BUTTON_DOWN) || (new_pad & BUTTON_LEFT) || (new_pad & BUTTON_RIGHT))
			{
				x_icon=100;
				c_option_number=option_number;
				if(option_number>5) { c_option_number=option_number-6; x_icon=1060;}
				put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_b_x*4 + dox_arrow_b_y	* dox_width*4), dox_arrow_b_w,	dox_arrow_b_h, dox_width, x_icon, (int)(y_icon + (135.f * (c_option_number))), 0, 0);
			}

			if ( (new_pad & BUTTON_DOWN))
			{
				int oloop;
				for(oloop=option_number; oloop<12; oloop++)
				{
					option_number++;
					if(oflags & (1<<option_number)) break;
				}
				if(option_number>11) option_number=0;
			}

			if ( (new_pad & BUTTON_RIGHT))
			{
				int oloop;
				option_number+=5;
				for(oloop=option_number; oloop<12; oloop++)
				{
					option_number++;
					if(oflags & (1<<option_number)) break;
				}
				if(option_number>11) option_number=0;
			}

			if ( (new_pad & BUTTON_UP))
			{
				int oloop;
				if(option_number==0) option_number=12;
				for(oloop=option_number; oloop>=0; oloop--)
				{
					option_number--;
					if(oflags & (1<<option_number)) break;
				}
				if(option_number<0) option_number=0;
			}

			if ( (new_pad & BUTTON_LEFT))
			{
				int oloop;
				option_number-=5;
				for(oloop=option_number; oloop>=0; oloop--)
				{
					option_number--;
					if(oflags & (1<<option_number)) break;
				}
				if(option_number<0) option_number=0;
			}

			if ( (new_pad & BUTTON_UP) || (new_pad & BUTTON_DOWN) || (new_pad & BUTTON_LEFT) || (new_pad & BUTTON_RIGHT))
			{
				x_icon=100;
				c_option_number=option_number;
				if(option_number>5) { c_option_number=option_number-6; x_icon=1060;}
				put_texture_with_alpha( text_FONT, text_DOX+(dox_arrow_w_x*4 + dox_arrow_w_y	* dox_width*4), dox_arrow_w_w,	dox_arrow_w_h, dox_width, x_icon, (int)(y_icon + (135.f * (c_option_number))), 0, 0);
			}

		}

		if ( (new_pad & BUTTON_CROSS) || (new_pad & BUTTON_L3) || (new_pad & BUTTON_R3)){
			result=option_number+1;
			//if(!is_locked && is_game) {	menu_list[*_game_sel].user=gflags;set_game_flags(*_game_sel); }
			//save changed options on main options activation
			break;
		}



		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

/*		mouseX+=mouseXD; mouseY+=mouseYD;
		if(mouseX>0.995f) {mouseX=0.995f;mouseXD=0.0f;} if(mouseX<0.0f) {mouseX=0.0f;mouseXD=0.0f;}
		if(mouseY>0.990f) {mouseY=0.990f;mouseYD=0.0f;} if(mouseY<0.0f) {mouseY=0.0f;mouseYD=0.0f;}
*/
//		b_box_opaq+=b_box_step;
//		if(b_box_opaq>0xfe) b_box_step=-1;
//		if(b_box_opaq<0x20) b_box_step= 2;
//		b_color = (b_color & 0xffffff00) | (b_box_opaq);
//		draw_square(((0.054f-0.5f)*2.0f)-0.005f, ((0.5f-(120.f+top_o)/1080.f)+0.005f)*2.0f , 0.675f, 0.68f, -0.4f, b_color);
		cellDbgFontPrintf( 0.99f, 0.98f, 0.5f,0x60606040, payloadT); 
		set_texture( text_FONT, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, -0.5f, 1920, 1080);

		//setRenderColor();
		//cellDbgFontDrawGcm();
//		draw_mouse_pointer(0);
		flip();
	}
	old_fi=-1; game_last_page=-1;
	new_pad=0; old_pad=0;
	counter_png=0;
	//sub_menu_open=0;

	if(result==4) //take screenshot
	{
		char string1[64];
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

		set_texture( buffer, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, -0.5f, 1920, 1080);
		//setRenderColor();
		//cellDbgFontDrawGcm();
//		draw_mouse_pointer(0);
		flip();
		sys_timer_usleep(250000);
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		char video_mem[64];
		sprintf(video_mem, "/dev_hdd0/%04d%02d%02d-%02d%02d%02d-SCREENSHOT.RAW", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
		FILE *fpA;
		remove(video_mem);
		fpA = fopen ( video_mem, "wb" );
		uint64_t c_pos=0;
		for(c_pos=0;c_pos<video_buffer;c_pos+=4){
			fwrite((uint8_t*)(color_base_addr)+c_pos+1, 3, 1, fpA);
		}
		fclose(fpA);
		if(stat("/dev_usb000",&s3)>=0){
			sprintf(string1, "/dev_usb000/%s", video_mem+10);
			file_copy(video_mem, string1, 0);
			remove(video_mem);
		}
		else
		if(stat("/dev_usb001",&s3)>=0){
			sprintf(string1, "/dev_usb001/%s", video_mem+10);
			file_copy(video_mem, string1, 0);
			remove(video_mem);
		}
	}

	return result;
}

/****************************************************/
/* UTILS                                            */
/****************************************************/

void fix_perm_recursive(const char* start_path)
{
	new_pad=0; old_pad=0;
	if(abort_rec==1) return;
	//cellSysutilCheckCallback();
	if(strstr(start_path,"/pvd_usb")!=NULL) return;

    int dir_fd;
    uint64_t nread;
    char f_name[CELL_FS_MAX_FS_FILE_NAME_LENGTH+1];
    CellFsDirent dir_ent;
    CellFsErrno err;
    cellFsChmod(start_path, 0777);
	//cellSysutilCheckCallback();
	flip();
    if (cellFsOpendir(start_path, &dir_fd) == CELL_FS_SUCCEEDED)
    {
        cellFsChmod(start_path, 0777);
        while (1) {
			pad_read();
			if ( (old_pad & BUTTON_CIRCLE) || (old_pad & BUTTON_TRIANGLE) || dialog_ret==3) { abort_rec=1; new_pad=0; old_pad=0; break; } //
            err = cellFsReaddir(dir_fd, &dir_ent, &nread);
            if (nread != 0) {
                if (!strcmp(dir_ent.d_name, ".") || !strcmp(dir_ent.d_name, ".."))
                    continue;

                sprintf(f_name, "%s/%s", start_path, dir_ent.d_name);

                if (dir_ent.d_type == CELL_FS_TYPE_DIRECTORY)
                {
                    cellFsChmod(f_name, CELL_FS_S_IFDIR | 0777);
                    fix_perm_recursive(f_name);
					if(abort_rec==1) break;
                }
                else if (dir_ent.d_type == CELL_FS_TYPE_REGULAR)
                {
                    cellFsChmod(f_name, 0666);
                }
            } else {
                break;
            }
        } 
        err = cellFsClosedir(dir_fd);
    }
}


int parse_ps3_disc(char *path, char * id)
{
	FILE *fp;
	int n;
	
	fp = fopen(path, "rb");
	if (fp != NULL)
		{
		unsigned len;
		unsigned char *mem=NULL;

		fseek(fp, 0, SEEK_END);
		len=ftell(fp);

		mem= (unsigned char *) malloc(len+16);
		if(!mem) {fclose(fp);return -2;}

		memset(mem, 0, len+16);

		fseek(fp, 0, SEEK_SET);

		fread((void *) mem, len, 1, fp);

		fclose(fp);

		for(n=0x20;n<0x200;n+=0x20)
			{
			if(!strcmp((char *) &mem[n], "TITLE_ID"))
				{
				n= (mem[n+0x12]<<8) | mem[n+0x13];
				memcpy(id, &mem[n], 16);
				id[4]=id[5];id[5]=id[6];id[6]=id[7];id[7]=id[8];id[8]=id[9];id[9]=0;
				return 0;
				}
			}
		}

	return -1;

}

static double get_system_version(void)
{
	FILE *fp;
	float base=3.41f;
	fp = fopen("/dev_flash/vsh/etc/version.txt", "rb");
	if (fp != NULL) {
		char bufs[1024];
		fgets(bufs, 1024, fp);
		fclose(fp);
		base = strtod(bufs + 8, NULL); // this is either the spoofed or actual version
		/*
		if(strstr(bufs, "20091206:tetsu@tetsu-linux18.rd.scei.sony.co.jp")!=NULL) base=3.15f;
		else if(strstr(bufs, "20100721:tetsu@tetsu-linux22.rd.scei.sony.co.jp")!=NULL) base=3.41f;
		else if(strstr(bufs, "20100913:tetsu@tetsu-linux14")!=NULL) base=3.50f;
		else if(strstr(bufs, "20101127:tetsu@tetsu-linux14")!=NULL) base=3.55f;
		else if(strstr(bufs, "20110129:tetsu@tetsu-linux16")!=NULL) base=3.56f;
		else if(strstr(bufs, "20110304:tetsu@tetsu-linux15")!=NULL) base=3.60f; */
	}

	fp = fopen("/dev_flash/sys/external/libfs.sprx", "rb");
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		uint32_t len = ftell(fp);
		unsigned char *mem = NULL;
		mem= (unsigned char *) malloc(len);
		fseek(fp, 0, SEEK_SET);
		fread((void *) mem, len, 1, fp);
		fclose(fp);
		uint32_t crc=0, crc_c;
		for(crc_c=0; crc_c<len; crc_c++) crc+=mem[crc_c];
//		sprintf(www_info, "%x", crc);
		if(crc==0x416bbaULL) base=3.15;	else
		if(crc==0x41721eULL) base=3.41; else
		if(crc==0x41655eULL) base=3.55;
		free(mem);
	}

	return base;
}

void change_param_sfo_field(char *file, char *field, char *value)
{
	if(stat(file, &s3)<0 || strstr(file, "/dev_bdvd")!=NULL) return;
	FILE *fp;
	cellFsChmod(file, 0666);
	fp = fopen(file, "rb");
	if (fp != NULL) {
		unsigned len, pos, str;
		unsigned char *mem = NULL;

		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		mem = (unsigned char *) malloc(len + 16);
		if (!mem) {
			fclose(fp);
			return;
		}

		memset(mem, 0, len + 16);

		fseek(fp, 0, SEEK_SET);
		fread((void *) mem, len, 1, fp);
		fclose(fp);

		str = (mem[8] + (mem[9] << 8));
		pos = (mem[0xc] + (mem[0xd] << 8));

		int indx = 0;

		while (str < len) {
			if (mem[str] == 0)
				break;

				if (!strcmp((char *) &mem[str], field) && mem[str+strlen(field)]==0) {
					memcpy(&mem[pos], value, strlen(value));
					mem[pos+strlen(value)]=0;
					fp = fopen(file, "wb");
					fwrite(mem, len, 1, fp);
					fclose(fp);
				}


			while (mem[str])
				str++;
			str++;
			pos += (mem[0x1c + indx] + (mem[0x1d + indx] << 8));
			indx += 16;
		}
		if (mem)
			free(mem);
	}
}


int get_param_sfo_field(char *file, char *field, char *value)
{
	if(stat(file, &s3)<0) return 0; // || strstr(file, "/dev_bdvd")!=NULL
	FILE *fp;
	cellFsChmod(file, 0666);
	fp = fopen(file, "rb");
	if (fp != NULL) {
		unsigned len, pos, str;
		unsigned char *mem = NULL;

		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		mem = (unsigned char *) malloc(len + 16);
		if (!mem) {
			fclose(fp);
			return 0;
		}

		memset(mem, 0, len + 16);

		fseek(fp, 0, SEEK_SET);
		fread((void *) mem, len, 1, fp);
		fclose(fp);

		str = (mem[8] + (mem[9] << 8));
		pos = (mem[0xc] + (mem[0xd] << 8));

		int indx = 0;

		while (str < len) {
			if (mem[str] == 0)
				break;

				if (!strcmp((char *) &mem[str], field) && mem[str+strlen(field)]==0) {
//					memcpy(&mem[pos], value, strlen(value));
					memcpy(value, &mem[pos], strlen(field));
					value[strlen(field)]=0;
					free(mem);
					return 1;
				}

			while (mem[str])
				str++;
			str++;
			pos += (mem[0x1c + indx] + (mem[0x1d + indx] << 8));
			indx += 16;
		}
		if (mem)
			free(mem);
	}
	return 0;
}


void change_param_sfo_version(const char *file) //parts from drizzt
{
	if(stat(file, &s3)<0 || strstr(file, "/dev_bdvd")!=NULL) return;
	FILE *fp;
	cellFsChmod(file, 0666);
	fp = fopen(file, "rb");
	if (fp != NULL) {
		unsigned len, pos, str;
		unsigned char *mem = NULL;

		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		mem = (unsigned char *) malloc(len + 16);
		if (!mem) {
			fclose(fp);
			return;
		}

		memset(mem, 0, len + 16);

		fseek(fp, 0, SEEK_SET);
		fread((void *) mem, len, 1, fp);
		fclose(fp);

		str = (mem[8] + (mem[9] << 8));
		pos = (mem[0xc] + (mem[0xd] << 8));

		int indx = 0;

		while (str < len) {
			if (mem[str] == 0)
				break;


				if (!strcmp((char *) &mem[str], "ATTRIBUTE")) {
					if ( (mem[pos] & 0x25) != 0x25) {
						mem[pos] |= 0x25;
						fp = fopen(file, "wb");
						if (fp != NULL) {
							fwrite(mem, len, 1, fp);
							fclose(fp);
						}
					}
				}


			if (!strcmp((char *) &mem[str], "PS3_SYSTEM_VER")) {
				float ver;
				ver = strtod((char *) &mem[pos], NULL);
				if (c_firmware < ver) {
					char msg[170];
					snprintf(msg, sizeof(msg),
							 "Game requires PS3 firmware version %.2f.\n\nDo you want to change PARAM.SFO version to %.2f?", ver, c_firmware);

					int t_dialog_ret=dialog_ret;
					dialog_ret = 0; cellMsgDialogAbort();
					cellMsgDialogOpen2(type_dialog_yes_no, msg, dialog_fun1, (void *) 0x0000aaaa, NULL);
					wait_dialog();
					if (dialog_ret == 1) {
						char ver_patch[10];
						//format the version to be patched so it is xx.xxx
						snprintf(ver_patch, sizeof(ver_patch), "%06.3f", c_firmware);
						memcpy(&mem[pos], ver_patch, 6);
						fp = fopen(file, "wb");
						if (fp != NULL) {
							fwrite(mem, len, 1, fp);
							fclose(fp);
						}
					}
					dialog_ret=t_dialog_ret;
				}
				break;
			}

			while (mem[str])
				str++;
			str++;
			pos += (mem[0x1c + indx] + (mem[0x1d + indx] << 8));
			indx += 16;
		}
		if (mem)
			free(mem);
	}
}


int parse_param_sfo(char *file, char *title_name, char *title_id, int *par_level)
{
//	if(strstr(file, "/pvd_usb")!=NULL) return -1;

	*par_level=0;

	FILE *fp = NULL;

	unsigned len, pos, str;
	unsigned char *mem = NULL;
#if (CELL_SDK_VERSION>0x210001)
	int pfsm=0;
	if(strstr(file, "/pvd_usb")!=NULL) pfsm=1;
	PFS_HFILE fh = PFS_FILE_INVALID;
	if (pfsm) {
		uint64_t size;
		if ((fh = PfsFileOpen(file)) == PFS_FILE_INVALID)
			return -1;
		if (PfsFileGetSizeFromHandle(fh, &size) != 0) {
			PfsFileClose(fh);
			return -1;
		}
		len = (unsigned)size;
	} else
#endif
	{
		if ((fp = fopen(file, "rb")) == NULL)
			return -1;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);
	}

	mem = (unsigned char *) malloc(len + 16);
	if (!mem) {
#if (CELL_SDK_VERSION>0x210001)
		if (pfsm) {
			PfsFileClose(fh);
		} else 
#endif
		{
			fclose(fp);
		}
		return -2;
	}

		memset(mem, 0, len+16);

#if (CELL_SDK_VERSION>0x210001)
	if (pfsm) {
		int32_t r;
		r = PfsFileRead(fh, mem, len, NULL);
		PfsFileClose(fh);
		if (r != 0)
			return -1;
	} else 
#endif
	{
		fseek(fp, 0, SEEK_SET);
		fread((void *) mem, len, 1, fp);
		fclose(fp);
	}

		str= (mem[8]+(mem[9]<<8));
		pos=(mem[0xc]+(mem[0xd]<<8));

		int indx=0;

		while(str<len)
			{
			if(mem[str]==0) break;
			
			if(!strcmp((char *) &mem[str], "TITLE"))
				{
				memset(title_name, 0, 63);
				strncpy(title_name, (char *) &mem[pos], 63);
//				free(mem);
				goto scan_for_PL;
				}
			while(mem[str]) str++;str++;
			pos+=(mem[0x1c+indx]+(mem[0x1d+indx]<<8));
			indx+=16;
			}

scan_for_PL:
		str= (mem[8]+(mem[9]<<8));
		pos=(mem[0xc]+(mem[0xd]<<8));
		indx=0;

		while(str<len)
			{
			if(mem[str]==0) break;
			
			if(!strcmp((char *) &mem[str], "PARENTAL_LEVEL"))
				{
				(*par_level)=mem[pos]; 
				goto scan_for_id;
				}
			while(mem[str]) str++;str++;
			pos+=(mem[0x1c+indx]+(mem[0x1d+indx]<<8));
			indx+=16;
			}

scan_for_id:
		str= (mem[8]+(mem[9]<<8));
		pos=(mem[0xc]+(mem[0xd]<<8));
		indx=0;

		while(str<len)
			{
			if(mem[str]==0) break;
			
			if(!strcmp((char *) &mem[str], "TITLE_ID"))
				{
				memset(title_id, 0, 10);
				strncpy(title_id, (char *) &mem[pos], 10); title_id[9]=0; 
				free(mem);
				return 0;
				}
			while(mem[str]) str++;str++;
			pos+=(mem[0x1c+indx]+(mem[0x1d+indx]<<8));
			indx+=16;
			}



		if(mem) free(mem);
	
	return -1;

}




void sort_pane(t_dir_pane *list, int *max)
{
	int n,m;
	int fi= (*max);
	if(fi<2) return;
	t_dir_pane swap;

	for(n=0; n< (fi -1);n++)
		for(m=n+1; m< fi ;m++)
			{
			if(strcasecmp(list[n].entry, list[m].entry)>0)
				{
					swap=list[n];list[n]=list[m];list[m]=swap;
				}
			}
}




/*
void delete_entries_content(t_menu_list *list, int *max, char *content_type)
{
	int n;

	n=0;

	while(n<(*max) )
		{
		if(list[n].content == content_type)
			{
			if((*max) >1)
				{
				list[n].flags=0;
				list[n]=list[(*max) -1];
				(*max) --;
				}
			else  {if((*max) == 1)(*max) --; break;}
			
			}
		else n++;

		}
}
*/

int ps3_home_scan(char *path, t_dir_pane *list, int *max)
{
	DIR  *dir;
	char *f= NULL;
	struct CellFsStat s;

   dir=opendir (path);
   if(!dir) return -1;
   
   while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;
		
		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;
		if(strstr(entry->d_name, ".MTH")!=NULL || strstr(entry->d_name, ".STH")!=NULL) continue;

		if((entry->d_type & DT_DIR))
			{
			
			char *d1f= (char *) malloc(512);
			if(!d1f) {closedir (dir);return -1;}
			sprintf(d1f,"%s/%s", path, entry->d_name);
			ps3_home_scan(d1f, list, max);
			free(d1f);
			}
		else
			{

			f=(char *) malloc(512);
			if(!f) {return -1;}
			sprintf(f,"%s/%s", path, entry->d_name);
			if( (search_mmiso==1 && strstr(f, ".mmiso.")!=NULL) || search_mmiso==0)
			{

				sprintf(list[*max ].name, "%s", entry->d_name);
				sprintf(list[*max ].path, "%s", f);
				sprintf(list[*max ].entry, "__1%s", entry->d_name); list[*max ].time=time(NULL); list[*max ].size=0; list[*max ].type=1;

				if(cellFsStat(list[*max ].path, &s)==CELL_FS_SUCCEEDED)
				{
					list[*max].size=s.st_size;
					list[*max].time=s.st_ctime;
				}

				(*max) ++;
			}


			if(f) free(f);
			}

		}

	closedir (dir);

	return 0;
}

int ps3_home_scan_ext(char *path, t_dir_pane *list, int *max, char *_ext)
{
	if((*max)>=MAX_PANE_SIZE) return 0;
	DIR  *dir;
	char *f= NULL;

   dir=opendir (path);
   if(!dir) return -1;
   
   while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;
		
		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;

		if((entry->d_type & DT_DIR))
			{
			
			char *d1f= (char *) malloc(512);
			if(!d1f) {closedir (dir);return -1;}
			sprintf(d1f,"%s/%s", path, entry->d_name);
			ps3_home_scan_ext(d1f, list, max, _ext);
			free(d1f);
			}
		else
			{

//			f=(char *) malloc(512);
//			if(!f) {return -1;}
//			sprintf(f,"%s/%s", path, entry->d_name);

			if(strstr(entry->d_name, _ext)==NULL) continue;

			sprintf(list[*max ].name, "%s", entry->d_name);
			sprintf(list[*max ].path, "%s", path);
			(*max) ++;
			if(f) free(f);
			if((*max)>=MAX_PANE_SIZE) break;
			}

		}

	closedir (dir);

	return 0;
}

int ps3_home_scan_ext_bare(char *path, t_dir_pane_bare *list, int *max, char *_ext)
{
	if((*max)>=MAX_PANE_SIZE_BARE) return 0;
	DIR  *dir;
	char *f= NULL;

   dir=opendir (path);
   if(!dir) return -1;
   
   while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;
		
		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;

		if((entry->d_type & DT_DIR))
			{
			
			char *d1f= (char *) malloc(512);
			if(!d1f) {closedir (dir);return -1;}
			sprintf(d1f,"%s/%s", path, entry->d_name);
			ps3_home_scan_ext_bare(d1f, list, max, _ext);
			free(d1f);
			}
		else
			{

//			f=(char *) malloc(512);
//			if(!f) {return -1;}
//			sprintf(f,"%s/%s", path, entry->d_name);

			if(strstr(entry->d_name, _ext)==NULL) continue;

			sprintf(list[*max ].name, "%s", entry->d_name);
			sprintf(list[*max ].path, "%s", path);
			(*max) ++;
			if(f) free(f);
			if((*max)>=MAX_PANE_SIZE_BARE) break;
			}

		}

	closedir (dir);

	return 0;
}


int ps3_home_scan_bare(char *path, t_dir_pane_bare *list, int *max)
{
	if((*max)>=MAX_PANE_SIZE_BARE) return 0;
	DIR  *dir;
	char *f= NULL;

   dir=opendir (path);
   if(!dir) return -1;
   
   while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;
		
		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;
		if(strstr(entry->d_name, ".MTH")!=NULL || strstr(entry->d_name, ".STH")!=NULL) continue;

		if((entry->d_type & DT_DIR))
			{
			
			char *d1f= (char *) malloc(512);
			if(!d1f) {closedir (dir);return -1;}
			sprintf(d1f,"%s/%s", path, entry->d_name);
			ps3_home_scan_bare(d1f, list, max);
			free(d1f);
			}
		else
			{

//			f=(char *) malloc(512);
//			if(!f) {return -1;}
//			sprintf(f,"%s/%s", path, entry->d_name);

			sprintf(list[*max ].name, "%s", entry->d_name);
			sprintf(list[*max ].path, "%s", path);
			(*max) ++;
			if(f) free(f);
			if((*max)>=MAX_PANE_SIZE_BARE) break;
			}

		}

	closedir (dir);

	return 0;
}

int ps3_home_scan_bare2(char *path, t_dir_pane *list, int *max)
{
	if((*max)>=MAX_PANE_SIZE) return 0;
	DIR  *dir;
	char *f= NULL;

   dir=opendir (path);
   if(!dir) return -1;
   
   while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;
		
		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;
		if(strstr(entry->d_name, ".MTH")!=NULL || strstr(entry->d_name, ".STH")!=NULL) continue;

		if((entry->d_type & DT_DIR))
			{
			
			char *d1f= (char *) malloc(512);
			if(!d1f) {closedir (dir);return -1;}
			sprintf(d1f,"%s/%s", path, entry->d_name);
			ps3_home_scan_bare2(d1f, list, max);
			free(d1f);
			}
		else
			{

//			f=(char *) malloc(512);
//			if(!f) {return -1;}
//			sprintf(f,"%s/%s", path, entry->d_name);

			sprintf(list[*max ].name, "%s", entry->d_name);
			sprintf(list[*max ].path, "%s", path);
			(*max) ++;
			if(f) free(f);
			if((*max)>=MAX_PANE_SIZE) break;
			}

		}

	closedir (dir);

	return 0;
}


void read_dir(char *path, t_dir_pane *list, int *max)
{
//	DIR  *dir;
	struct CellFsStat s;
	*max =0;

	FILE *fp;
	int n=0, foundslash=0, slashpos=0;
	char net_host_file[512], net_host_file2[512], tempname[512], tempname2[512], tempname3[8];
	char net_path_bare [512], title[512], date2[10], timeC[10], type[1], net_path[512], parent[512];
	char path2[512], path3[512], path4[512];
	char temp[3], length[128];
	time_t c_time = time(NULL);
	if(strstr(path,"/net_host")==NULL) goto regular_FS_PS;
	sprintf(net_host_file2, "%s", path); net_host_file2[10]=0;
//	strncpy(net_host_file2, path, 10); net_host_file2[10]=0;
    sprintf(net_host_file,"%s%s", app_usrdir, net_host_file2);

	// add root shortcut to regular file system
	// in case something is wrong with the net_host
	list[*max].type=0;
	sprintf(list[*max ].name, "/");
	sprintf(list[*max ].path, "/");
	sprintf(list[*max ].entry, "     ");
	list[*max].size=0; list[*max].time=time(NULL);
	timeinfo = localtime ( &c_time );
	if(date_format==0) sprintf(list[*max].datetime, "%02d/%02d/%04d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
	else if(date_format==1) sprintf(list[*max].datetime, "%02d/%02d/%04d", timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_year+1900);
	else if(date_format==2) sprintf(list[*max].datetime, "%04d/%02d/%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday );
	(*max) ++;

	fp = fopen ( net_host_file, "r" );
	if ( fp != NULL ) 

	{ 
		sprintf(path2, "%s/", path);
		sprintf(path3, "%s/0", path);

		while (fscanf(fp,"%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%s\n", net_path_bare, title, length, timeC, date2, type, parent, tempname3)>=7) {


			sprintf(net_path, "%s%s", net_host_file2, net_path_bare);
			sprintf(path4, "%s%s%s", net_host_file2, parent, type);


			if(strcmp(path3, path4)==0)
			{

				list[*max].type=0;

				strncpy(tempname, net_path_bare+1, strlen(net_path_bare)-2); tempname[strlen(net_path_bare)-2]=0;
				foundslash=0; slashpos=0; int pl=strlen(tempname);
				for(n=pl;n>1;n--)
				{
					if(tempname[n]==0x2F) { foundslash=n; break; }
				}
				for(n=0;n<pl;n++)
				{	if(n>foundslash && foundslash>0)
					{
					tempname2[slashpos]=tempname[n];
					slashpos++;
					tempname2[slashpos]=0;
					}
				}
				if(foundslash==0) sprintf(tempname2, "%s%c", tempname, 0);
//				utf8_to_ansi(tempname2, list[*max ].name, 128);
				sprintf(list[*max ].name, "%s", tempname2);

				strncpy(list[*max ].path, net_path, strlen(net_path)-1); list[*max ].path[strlen(net_path)-1]=0;
				sprintf(list[*max ].entry, "__0%s", net_path_bare);
				list[*max].size=0;
				list[*max].time=0;
				list[*max].mode=0;
				list[*max].selected=0;
				sprintf(list[*max].datetime, "%s%c", " ", 0);

				(*max) ++;
				if(*max >=2048) break;

			} //subfolder

			if(strcmp(net_path, path2)==0)
			{

				list[*max].type=strtol(type, NULL, 10);
//				utf8_to_ansi(title, list[*max ].name, 128);
				sprintf(list[*max ].name, "%s%c", title, 0);				
//				strncpy(list[*max ].name, title, 128); 
				list[*max ].name[128]=0;
				sprintf(list[*max ].path, "%s/%s", path, title);

				temp[0]=0x2e; temp[1]=0x2e; temp[2]=0;
				if(strcmp(temp, title)==0)
				{
					sprintf(list[*max ].path, "%s", path);
					char *pch=list[*max ].path;
					char *pathpos=strrchr(pch,'/');	int lastO=pathpos-pch;
					list[*max ].path[lastO]=0; 
				}

				sprintf(list[*max ].entry, "__%i%s", list[*max].type, title);

				list[*max].size=strtoull(length, NULL, 10);
				list[*max].time=0;
				list[*max].mode=0;
				list[*max].selected=0;
				sprintf(list[*max].datetime, "%s", date2);
				if(strlen(date2)>9 && date_format>0)
				{
					if(date_format==1)  // 01/34/6789
						sprintf(list[*max].datetime, "%c%c/%c%c/%c%c%c%c", date2[3], date2[4], date2[0], date2[1], date2[6], date2[7], date2[8], date2[9]);
					else if(date_format==2)  // 01/34/6789
						sprintf(list[*max].datetime, "%c%c%c%c/%c%c/%c%c", date2[6], date2[7], date2[8], date2[9], date2[3], date2[4], date2[0], date2[1]);

				}




//			sprintf(filename, "Path: [%s]\nInfo [%f] [%s] [%f]", list[*max].path, list[*max].size/1.0f, length, strtol(length, NULL, 10)/1.0f); dialog_ret=0; cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );wait_dialog();

				(*max) ++;
				if(*max >=2048) break;

			}

		} //while
		fclose(fp);
	}


	goto finalize;

regular_FS_PS:

	if(strstr(path,"/ps3_home")==NULL) goto regular_FS_NTFS;

	if(strcmp(path,"/ps3_home")==0){
			sprintf(list[*max ].name, "..");
			sprintf(list[*max ].path, "/"); list[*max].mode=0;list[*max].selected=0;
			sprintf(list[*max ].entry, "     "); list[*max ].time=time(NULL); list[*max ].size=0; list[*max ].type=0; (*max) ++;

			sprintf(list[*max ].name, "music");
			sprintf(list[*max ].path, "/ps3_home/music"); list[*max].mode=0;list[*max].selected=0;
			sprintf(list[*max ].entry, "__0music");	list[*max ].time=time(NULL); list[*max ].size=0;	list[*max ].type=0;	(*max) ++;

			sprintf(list[*max ].name, "photo");
			sprintf(list[*max ].path, "/ps3_home/photo"); list[*max].mode=0;list[*max].selected=0;
			sprintf(list[*max ].entry, "__0photo");	list[*max ].time=time(NULL); list[*max ].size=0;	list[*max ].type=0;	(*max) ++;

			sprintf(list[*max ].name, "video");
			sprintf(list[*max ].path, "/ps3_home/video"); list[*max].mode=0;list[*max].selected=0;
			sprintf(list[*max ].entry, "__0video");	list[*max ].time=time(NULL); list[*max ].size=0;	list[*max ].type=0;	(*max) ++;

			sprintf(list[*max ].name, "archive");
			sprintf(list[*max ].path, "/ps3_home/mmiso"); list[*max].mode=0;list[*max].selected=0;
			sprintf(list[*max ].entry, "__0archive");	list[*max ].time=time(NULL); list[*max ].size=0;	list[*max ].type=0;	(*max) ++;
	}

	if(strstr(path,"/ps3_home/")!=NULL){
		sprintf(list[*max ].name, "..");
		sprintf(list[*max ].path, "/ps3_home"); list[*max].mode=0;list[*max].selected=0;
		sprintf(list[*max ].entry, "     "); list[*max ].time=time(NULL); list[*max ].size=0; list[*max ].type=0; (*max) ++;
	}

	search_mmiso=0;
	if(strstr(path,"/ps3_home/music")!=NULL)	ps3_home_scan((char *)"/dev_hdd0/music", list, max); 
	if(strstr(path,"/ps3_home/video")!=NULL)	ps3_home_scan((char *)"/dev_hdd0/video", list, max); 
	if(strstr(path,"/ps3_home/photo")!=NULL)	ps3_home_scan((char *)"/dev_hdd0/photo", list, max); 
	if(strstr(path,"/ps3_home/mmiso")!=NULL)	{search_mmiso=1; ps3_home_scan((char *)"/dev_hdd0/video", list, max); search_mmiso=0;}

	goto finalize;

regular_FS_NTFS:
	char *pch;
	char *pathpos;
	int lastO;
#if (CELL_SDK_VERSION>0x210001)
	if(strstr(path,"/pvd_usb")==NULL) goto regular_FS;

	PFS_HFIND dir;
	PFS_FIND_DATA entryP;

	sprintf(list[*max ].path, "%s", path);
	pch=list[*max ].path;
	pathpos=strrchr(pch,'/'); lastO=pathpos-pch;
	list[*max ].path[lastO]=0; 
	sprintf(list[*max ].entry, "     ");
	sprintf(list[*max ].name, "..");
	list[*max].type=0;
	(*max) ++;

	dir = PfsFileFindFirst(path, &entryP);
	if (!dir)
		goto regular_FS;

	do {
		if (!strcmp(entryP.FileName, ".") || !strcmp(entryP.FileName, ".."))
			continue;

		if (entryP.FileAttributes & PFS_FIND_DIR)
			list[*max].type=0;
		else
			list[*max].type=1;
	
		strncpy(list[*max ].name, entryP.FileName, 128); 
		list[*max ].name[128]=0;
		if(strlen(path)==1)
			sprintf(list[*max ].path, "/%s", entryP.FileName);
		else
			sprintf(list[*max ].path, "%s/%s", path, entryP.FileName);


		sprintf(list[*max ].entry, "__%i%s", list[*max].type, entryP.FileName);

		list[*max].time=time(NULL);;
		list[*max].size=entryP.FileSize;
		list[*max].mode=entryP.FileAttributes;
		list[*max].selected=0;

/*		if (list[*max].type==1)
		{
			PFS_HFILE fh = PFS_FILE_INVALID;
			uint64_t size;
			list[*max].size=0;

			if ((fh = PfsFileOpen(list[*max].path)) != PFS_FILE_INVALID) {
				if (PfsFileGetSizeFromHandle(fh, &size) == 0) {
								list[*max].size=size;
				}
			}
			if (fh != PFS_FILE_INVALID) PfsFileClose(fh);

		}

*/
		//list[*max].time=s.st_ctime;
		//if(s.st_mtime>0) list[*max].time=s.st_mtime;

		if(list[*max ].name[0]!=0x24 && strstr(list[*max ].name, "System Volume")==NULL) (*max) ++;
		if(*max >=2048) break;


	} while (PfsFileFindNext(dir, &entryP) == 0);
	PfsFileFindClose(dir);

	goto finalize;

regular_FS:
#endif

    int dir_fd;
    uint64_t nread;
    CellFsDirent entry;

//	dir=opendir (path);
    if (cellFsOpendir(path, &dir_fd) == CELL_FS_SUCCEEDED){
	while(1) {
//	while(dir) {
//		struct dirent *entry=readdir (dir);

		cellFsReaddir(dir_fd, &entry, &nread);
		if(nread==0) break;
//		if(!entry) break;


		if(entry.d_name[0]=='.' && entry.d_name[1]==0) continue;
		if(strstr(entry.d_name,"host_root")!=NULL) continue;
		if(!(entry.d_type & DT_DIR)) 
		{ 
			list[*max].type=1;
		}
		else
		{
			list[*max].type=0;
		}

		list[*max].mode=0;
		list[*max].selected=0;
//		utf8_to_ansi(entry.d_name, list[*max ].name, 128);
		strncpy(list[*max ].name, entry.d_name, 128); 
		list[*max ].name[128]=0;
		if(strlen(path)==1)
			sprintf(list[*max ].path, "/%s", entry.d_name);
		else
			sprintf(list[*max ].path, "%s/%s", path, entry.d_name);

		if(entry.d_name[0]=='.' && entry.d_name[1]=='.' && entry.d_name[2]==0)
		{
			sprintf(list[*max ].path, "%s", path);
			pch=list[*max ].path;
			pathpos=strrchr(pch,'/'); lastO=pathpos-pch;
			list[*max ].path[lastO]=0; 
			sprintf(list[*max ].entry, "     ");
		}
		else
			sprintf(list[*max ].entry, "__%i%s", list[*max].type, entry.d_name);


/*		
		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(!(entry->d_type & DT_DIR)) 
		{ 
			list[*max].type=1;
		}
		else
		{
			list[*max].type=0;
		}

		strncpy(list[*max ].name, entry->d_name, 128); list[*max ].name[128]=0;
		if(strlen(path)==1)
			sprintf(list[*max ].path, "/%s", entry->d_name);
		else
			sprintf(list[*max ].path, "%s/%s", path, entry->d_name);

		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0)
		{
			sprintf(list[*max ].path, "%s", path);
			char *pch=list[*max ].path;
			char *pathpos=strrchr(pch,'/');	int lastO=pathpos-pch;
			list[*max ].path[lastO]=0; 
			sprintf(list[*max ].entry, "     ");
		}
		else
			sprintf(list[*max ].entry, "__%i%s", list[*max].type, entry->d_name);

*/
		if(cellFsStat(list[*max ].path, &s)==CELL_FS_SUCCEEDED)
		{
			list[*max].size=s.st_size;
			list[*max].time=s.st_ctime;
			if(s.st_mtime>0) list[*max].time=s.st_mtime;
			list[*max].mode=s.st_mode;
		}

		(*max) ++;
		if(*max >=2048) break;

	} //while
	}
//	closedir(dir);
	cellFsClosedir(dir_fd);

finalize:

	if(*max==0)
	{
		sprintf(list[*max ].name, "/");
		sprintf(list[*max ].path, "/");
		sprintf(list[*max ].entry, "     ");
		list[*max ].time=0; list[*max ].size=0;
		list[*max ].type=0;
		(*max) ++;
	}

	temp[0]=0x2f; temp[1]=0x00; temp[2]=0;
	if(strcmp(temp, path)==0)
	{
		for(n=0;n<max_hosts;n++)
		{
			sprintf(list[*max ].name, "net_host%i %s - %s:%i", n, host_list[n].friendly, host_list[n].host, host_list[n].port);
			sprintf(list[*max ].path, "/net_host%i",n);
			sprintf(list[*max ].entry, "__0net_host%i", n);
			list[*max ].time=time(NULL); list[*max ].size=0;
			timeinfo = localtime ( &c_time );
			if(date_format==0) sprintf(list[*max].datetime, "%02d/%02d/%04d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
			else if(date_format==1) sprintf(list[*max].datetime, "%02d/%02d/%04d", timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_year+1900);
			else if(date_format==2) sprintf(list[*max].datetime, "%04d/%02d/%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday );

			list[*max ].type=0;
			(*max) ++;
		}

			sprintf(list[*max ].name, "ps3_home");
			sprintf(list[*max ].path, "/ps3_home");
			sprintf(list[*max ].entry, "__0ps3_home");
			list[*max ].time=time(NULL); list[*max ].size=0;
			list[*max ].type=0;
			(*max) ++;

#if (CELL_SDK_VERSION>0x210001)
	if(pfs_enabled) {
	int fsVol=0;
	for(fsVol=0;fsVol<(max_usb_volumes);fsVol++)
	{
		if (PfsmVolStat(fsVol) == 0) 
		{
			sprintf(list[*max ].name, "pvd_usb%i", fsVol);
			sprintf(list[*max ].path, "/pvd_usb00%i", fsVol);
			sprintf(list[*max ].entry, "__0pvd_usb00%i", fsVol);
			list[*max ].time=time(NULL); list[*max ].size=0;
			list[*max ].type=0;
			(*max) ++;
		}
	}
	}
#endif


/*			sprintf(list[*max ].name, "sys_cache");
			sprintf(list[*max ].path, "%s", sys_cache);
			sprintf(list[*max ].entry, "__0sys_cache");
			list[*max ].time=time(NULL); list[*max ].size=0;
			list[*max ].type=0;
			(*max) ++; */

	}

	sort_pane(list, max );
}

#if (CELL_SDK_VERSION>0x210001)
void fill_entries_from_device_pfs(char *path, t_menu_list *list, int *max, u32 flag, int sel)
{
	if(!pfs_enabled || is_reloaded) return;
	is_game_loading=1;
	if(sel!=2) delete_entries(list, max, flag);

    load_texture(text_bmpIC, blankBG, 320);
	//reset_xmb(1);
	PFS_HFIND dir;
	PFS_FIND_DATA entry;

	char file[1024];
	int skip_entry=0;
	char string2[1024];
	(void) sel;

	if ((*max) < 0)
		*max = 0;

//	sprintf(string2, "Will check dir: [%s]", path); dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, string2, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
//	if (PfsFileGetInfo(path, &info) != 0) return;
//	sprintf(string2, "Will read dir: [%s]", path); dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, string2, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
	dir = PfsFileFindFirst(path, &entry);
//	sprintf(string2, "READ dir: [%s]", path); dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, string2, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
	if (!dir) {	is_game_loading=0;	return; }


	do {
		if (!strcmp(entry.FileName, ".") ||
			!strcmp(entry.FileName, ".."))
			continue;

		if (!(entry.FileAttributes & PFS_FIND_DIR))
			continue;

		if(skip_entry==0 && cover_mode!=8) 
		{
			sprintf(string2, "Scanning, please wait!\n\n[%s/%s]",path, entry.FileName);
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

			set_texture( text_FMS, 1920, 48); display_img(0, 47, 1920, 60, 1920, 48, -0.15f, 1920, 48);	display_img(0, 952, 1920, 76, 1920, 48, -0.15f, 1920, 48);time ( &rawtime );	timeinfo = localtime ( &rawtime );	cellDbgFontPrintf( 0.83f, 0.89f, 0.7f ,0xc0a0a0a0, "%02d/%02d/%04d\n %s:%02d:%02d ", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, tmhour(timeinfo->tm_hour), timeinfo->tm_min, timeinfo->tm_sec);
			set_texture( text_bmpIC, 320, 320);  display_img(800, 200, 320, 176, 320, 176, 0.0f, 320, 320);

			cellDbgFontPrintf( 0.3f, 0.45f, 0.8f, 0xc0c0c0c0, string2);
			//if(first_launch) cellDbgFontPrintf( 0.30f, 0.80f, 1.0f, 0x90909090, multi_loading);
			flip(); 
		}
		skip_entry++; if( skip_entry>10) skip_entry=0; //(first_launch && skip_entry>3 ) || 

		list[*max].flags = flag;

		strncpy(list[*max].title, entry.FileName, 63);
		list[*max].title[63] = 0;

		sprintf(list[*max].path, "%s/%s", path, entry.FileName);
		sprintf(list[*max ].content, "%s", "PS3");
		sprintf(list[*max ].title_id, "%s", "NO_ID");
		list[*max ].split=0;
		list[*max ].user=IS_PS3;

		sprintf(file, "%s/PS3_GAME/PARAM.SFO", list[*max].path);

		parse_param_sfo(file, list[*max ].title+1*(list[*max ].title[0]=='_'), list[*max ].title_id, &list[*max ].plevel); // move +1 with '_' 
		list[*max ].title[63]=0;

		sprintf(file, "%s/PS3_GAME/PIC1.PNG", list[*max].path);
		sprintf(string2, "%s/%s_320.PNG", cache_dir, list[*max ].title_id);
		if(stat(string2, &s3)<0) {cache_png(file, list[*max ].title_id);}

		(*max)++;

		if (*max >= MAX_LIST)
			break;

	} while (PfsFileFindNext(dir, &entry) == 0);

	PfsFileFindClose(dir);
	//reset_xmb(1);
	is_game_loading=0;
}
#endif

void check_usb_ps3game(const char *path)
{
		struct stat s;
		if(strstr(path,"/dev_usb")!=NULL) { //check for PS3_GAME mount on external USB

						char usb_mount1[512], usb_mount2[512], path_bup[512], tempname[512];
						int pl, n;
						FILE *fpA;
						strncpy(tempname, path, 11); tempname[11]=0;
						sprintf(usb_mount1, "%s/PS3_GAME", tempname);

						if(stat(usb_mount1, &s)>=0) 
						{
							//restore PS3_GAME back to USB game folder
							sprintf(path_bup, "%s/PS3PATH.BUP", usb_mount1);
							if(stat(path_bup, &s)>=0) {
								fpA = fopen ( path_bup, "r" );
								if(fpA==NULL) goto continue_scan;
								if(fgets ( usb_mount2, 512, fpA )==NULL) goto cancel_move;
								fclose(fpA);
								strncpy(usb_mount2, path, 11); //always use current device

							if(stat(usb_mount2, &s3)<0)
							{
								pl=strlen(usb_mount2);
								for(n=0;n<pl;n++)
								{
									tempname[n]=usb_mount2[n];
									tempname[n+1]=0;
									if(usb_mount2[n]==0x2F && stat(tempname, &s)<0) 
									{	
										mkdir(tempname, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(tempname, 0777);
									}
								}

								rename (usb_mount1, usb_mount2);
							}

							goto continue_scan;
cancel_move:
							fclose(fpA);
							}
						}
	}

continue_scan:
	return;
}

void fill_entries_from_device(const char *path, t_menu_list *list, int *max, u32 flag, int sel)
{
	is_game_loading=1;
	struct stat s;
	check_usb_ps3game(path);
	is_game_loading=0;
//	reset_xmb(1);
	if(is_reloaded==2 || (is_reloaded==1 && strstr(path, "/dev_hdd")==NULL) ) return;

	is_game_loading=1;

	DIR  *dir;//, *dir2, *dir3;
	char file[1024], string2[1024];
	char path2[1024], path3[1024], avchd_path[12], detailsfile[512];
	int skip_entry=0;

	FILE *fp;
	char BDtype[6];

	if(sel!=2)
	delete_entries(list, max, flag);

	if((*max) <0) *max =0;
	char title[256], length[24], video[24], audio[24], web[256];

	sprintf(string2, "%s/AVCHD_240.RAW", cache_dir);
	if(stat(string2, &s3)<0) 
	{
		sprintf(string2, "%s", "AVCHD");
		cache_png(string2, string2);
	}

	if(first_launch) {		sprintf(string2, "%s/PRB.PNG", app_usrdir);
		load_texture(text_FMS, string2, 858);}


	if(scan_avchd==1 && strstr(path,"/dev_usb")!=NULL && sel==0 && (display_mode==0 || display_mode==2)) { // 
	strncpy(avchd_path, path, 11); avchd_path[11]=0;

	dir=opendir (avchd_path);

	while(dir)
		{
		pb_step-=10; if(pb_step<0) pb_step=429;
		if(first_launch)
		{
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			put_texture( text_bmpUBG, text_FMS+(pb_step*4), 429, 20, 858, 745, 840, 0, 0);
			set_texture( text_bmpUBG, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
			flip();
		}

		struct dirent *entry=readdir (dir);
		if(!entry) break;
		if(entry->d_name[0]=='.') continue;
		if(!(entry->d_type & DT_DIR)) continue;
		
		if((skip_entry==0 || cover_mode==8) && !first_launch) {
			//cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			//if(first_launch)
			//{
//				put_texture( text_bmpUBG, text_FMS+(pb_step*4), 429, 20, 858, 745, 840, 0, 0);
//				set_texture( text_bmpUBG, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//				sprintf(string2, "Scanning for AVCHD content [%s/%s], please wait...", avchd_path, entry->d_name);
//				cellDbgFontPrintf( (1.0f-(strlen(string2)*0.00625f))/2.0f, 0.80f, 0.5f, 0x90909090, string2);
//				cellDbgFontDrawGcm();	
//				max_ttf_label=0;
//				print_label_ex( 0.5f, 0.8, 0.5f, 0x90909090, string2, 1.0f, 0.0f, 15, 1.0f, 1.0f, 1);
//				cellDbgFontPrintf( 0.98f, 0.98f, 0.5f, 0x80808080, "A");
//				cellDbgFontDrawGcm();	
//			}
//			else
//			{
			if(cover_mode!=8)
				//draw_xmb_bare(xmb_icon, 2, 0, 0);
				//draw_whole_xmb(1);
			//else
			{
				cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
				sprintf(string2, "Scanning for AVCHD content, please wait!\n\n[%s/%s]",avchd_path, entry->d_name);
				set_texture( text_FMS, 1920, 48); display_img(0, 47, 1920, 60, 1920, 48, -0.15f, 1920, 48);	display_img(0, 952, 1920, 76, 1920, 48, -0.15f, 1920, 48);time ( &rawtime );	timeinfo = localtime ( &rawtime );	cellDbgFontPrintf( 0.83f, 0.895f, 0.7f ,0xc0a0a0a0, "%02d/%02d/%04d\n %s:%02d:%02d ", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, tmhour(timeinfo->tm_hour), timeinfo->tm_min, timeinfo->tm_sec);
				set_texture( text_bmpIC, 320, 320);  display_img(800, 200, 320, 176, 320, 176, 0.0f, 320, 320);

				cellDbgFontPrintf( 0.3f, 0.45f, 0.8f, 0xc0c0c0c0, string2);
				//if(first_launch) cellDbgFontPrintf( 0.30f, 0.80f, 1.0f, 0x90909090, multi_loading);
				cellDbgFontDrawGcm();	
//			}
				flip();
			}
		}
		skip_entry++; if(skip_entry>20) skip_entry=0;

		sprintf(path2, "%s/%s", avchd_path, entry->d_name); //dev_usb00x/AVCHD_something

		sprintf(file, "%s/BDMV/INDEX.BDM", path2); // /dev_usb00x/AVCHD_something/BDMV/INDEX.BDM
		sprintf(BDtype,"AVCHD");
		if(stat(file, &s)<0) {
			sprintf(file, "%s/BDMV/index.bdmv", path2); // /dev_usb00x/something/BDMV/index.bdmv
			if(stat(file, &s)>=0) sprintf(BDtype,"BDMV");}

		if(stat(file, &s)>=0)
			{

			char is_multiAVCHD[13];is_multiAVCHD[0]=0;
			sprintf(detailsfile, "%s/multiAVCHD.mpf", path2);
			if(stat(detailsfile, &s)>=0) sprintf(is_multiAVCHD, "%s", " (multiAVCHD)"); 
			is_multiAVCHD[13]=0;

			sprintf(path3, "[Video] %s%s", entry->d_name, is_multiAVCHD); path3[63]=0;
			sprintf(list[*max ].title, "%s", path3);
			list[*max ].flags=flag;
			sprintf(list[*max ].title_id, "%s", "AVCHD");
			sprintf(list[*max ].path, "%s", path2);
			sprintf(list[*max ].entry, "%s", entry->d_name);
			sprintf(list[*max ].content, "%s", BDtype);
			list[*max ].cover=-1;
			list[*max ].split=0;
			list[*max ].user=0;

	sprintf(detailsfile, "%s/details.txt", path2);
	fp = fopen ( detailsfile, "r" );
	if ( fp != NULL ) 
	{
		fseek (fp, 0, SEEK_SET);
		char lines[2048]="/"; lines[1]=0;
		int cline=0;
		while (fscanf(fp,"%[^;];%[^;];%[^;];%[^;];%s\n", title, length, video, audio, web)==5)
		{ 
				cline++;
				if(expand_avchd==1) 
				{
					sprintf(list[*max].title, "[Video] %s", title);
//					utf8_to_ansi(string2, list[*max].title, 63);
					list[*max].title[63]=0;

					list[*max ].flags=flag;
					sprintf(list[*max ].title_id, "%s", "AVCHD");
					sprintf(list[*max ].path, "%s", path2);
					sprintf(list[*max ].entry, "%s", entry->d_name);
					sprintf(list[*max ].content, "%s", BDtype);
					sprintf(list[*max ].details, "Duration: %s, Video: %s, Audio: %s", length, video, audio);
					list[*max ].cover=-1;
					(*max) ++;
				}
				else
				{
					if(cline==1) {
						is_multiAVCHD[13]=0;
						sprintf(string2, "[Video] %s%s", title, is_multiAVCHD); string2[62]=0;
						sprintf(list[*max].title, "%s", string2);
//						utf8_to_ansi(string2, list[*max].title, 58);list[*max].title[58]=0;
						}
					else
						sprintf(lines, "%s %s /", lines, title);
				}

				if(*max >=MAX_LIST) break;
		}
		lines[100]=0;
		if(expand_avchd==0) 
			{lines[90]=0; sprintf(list[*max].details, "%s", lines);}
//			{utf8_to_ansi(lines, list[*max].details, 90);list[*max].details[90]=0;}
		else {if(cline>0) (*max) --;}
		fclose ( fp );
	}

			(*max) ++; 
			} // INDEX.BDM found
		} // while
		closedir (dir);
	} // scan AVCHD


	dir=opendir (path);
	if(!dir) {is_game_loading=0; return;}

	if(sel==2) sel=0;


	while(1)
		{
		pb_step-=10; if(pb_step<0) pb_step=429;
		if(first_launch)
		{
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			put_texture( text_bmpUBG, text_FMS+(pb_step*4), 429, 20, 858, 745, 840, 0, 0);
			set_texture( text_bmpUBG, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
			flip();
		}
		struct dirent *entry=readdir (dir);
		if(!entry) break;
		if(entry->d_name[0]=='.') continue;
		if(!(entry->d_type & DT_DIR)) continue;

		sprintf(path2, "%s/%s", path, entry->d_name);
		sprintf(file, "%s/PS3_GAME/ICON0.PNG", path2);
		if(strcmp(path, "/dev_hdd0/game")==0)
		{
			sprintf(file, "%s/ICON0.PNG", path2);
		}

/*		if(skip_entry==0 || first_launch) 
		{
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			if(first_launch)
			{
				put_texture( text_bmpUBG, text_FMS+(pb_step*4), 429, 20, 858, 745, 840, 0, 0);
				set_texture( text_bmpUBG, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//				sprintf(string2, "Scanning [%s/%s], please wait...", path, entry->d_name);
//				cellDbgFontPrintf( (1.0f-(strlen(string2)*0.00625f))/2.0f, 0.80f, 0.5f, 0x90909090, string2);
//				cellDbgFontDrawGcm();	
//				max_ttf_label=0;
//				cellDbgFontPrintf( 0.98f, 0.98f, 0.5f, 0x80808080, "G");
//				print_label_ex( 0.5f, 0.8, 0.5f, 0x90909090, string2, 1.0f, 0.0f, 15, 1.0f, 1.0f, 1);


			}
			else */
		if(skip_entry==0 && !first_launch) 
		{
			if(strstr(path, "/dev_hdd0")!=NULL && stat(file, &s)>=0 && cover_mode!=8) load_texture(text_bmpIC, file, 320);

			if(cover_mode!=8)
				//draw_whole_xmb(1);
				//draw_xmb_bare(xmb_icon, 2, 0, 0);
				//max_ttf_label+=0;
			//else
			{
				sprintf(string2, "Scanning, please wait!\n\n[%s/%s]", path, entry->d_name);
				cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
				set_texture( text_FMS, 1920, 48); display_img(0, 47, 1920, 60, 1920, 48, -0.15f, 1920, 48);	display_img(0, 952, 1920, 76, 1920, 48, -0.15f, 1920, 48);time ( &rawtime );	timeinfo = localtime ( &rawtime );	cellDbgFontPrintf( 0.83f, 0.895f, 0.7f ,0xc0a0a0a0, "%02d/%02d/%04d\n %s:%02d:%02d ", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, tmhour(timeinfo->tm_hour), timeinfo->tm_min, timeinfo->tm_sec);
				set_texture( text_bmpIC, 320, 320);  display_img(800, 200, 320, 176, 320, 176, 0.0f, 320, 320);

				cellDbgFontPrintf( 0.3f, 0.45f, 0.8f, 0xc0c0c0c0, string2);
				//if(first_launch) cellDbgFontPrintf( 0.30f, 0.80f, 1.0f, 0x90909090, multi_loading);
				cellDbgFontDrawGcm();	
				flip(); 
			}
		}
		
		skip_entry++; if(skip_entry>10) skip_entry=0; // (first_launch && skip_entry>3 ) || 

//		sprintf(file, "%s/PS3_GAME/USRDIR/EBOOT.BIN", path2);
		sprintf(file, "%s/PS3_GAME/PARAM.SFO", path2);
		if(strcmp(path, "/dev_hdd0/game")==0)
		{
//			sprintf(file, "%s/USRDIR/MM_NON_NPDRM_EBOOT.BIN", path2);
//			if(stat(file, &s)>=0) sprintf(file, "%s/PARAM.SFO", path2);
//			else 
//			{
				sprintf(file, "%s/USRDIR/RELOAD.SELF", path2);
				if(stat(file, &s)>=0 && strstr(file, app_usrdir)==NULL) sprintf(file, "%s/PARAM.SFO", path2);
				else continue;
//			}
		}

/*		if(sel==1)
			{
			sprintf(file, "%s/EBOOT.BIN", path2);
			if(stat(file, &s)<0) sprintf(file, "%s/USRDIR/EBOOT.BIN", path2);
			if(stat(file, &s)<0) continue;
			}
*/
//		if(display_mode==0 || display_mode==1)
		if(display_mode!=2 && stat(file, &s)>=0) { 
	
		list[*max ].flags=flag;
		strncpy(list[*max ].title, entry->d_name, 63);
		list[*max ].title[63]=0;
		list[*max ].cover=0;

		sprintf(list[*max ].path, "%s/%s", path, entry->d_name);

		sprintf(list[*max ].entry, "%s", entry->d_name);
		sprintf(list[*max ].content, "%s", "PS3");
		sprintf(list[*max ].title_id, "%s", "NO_ID");
		list[*max ].split=0;
		list[*max ].user=IS_PS3;

		if(sel==0)
			{
				parse_param_sfo(file, list[*max ].title+1*(list[*max ].title[0]=='_'), list[*max ].title_id, &list[*max ].plevel); // move +1 with '_' 
				list[*max ].title[63]=0;

				if(strcmp(path, "/dev_hdd0/game")==0)
					sprintf(file, "%s/PIC1.PNG", path2);
				else
					sprintf(file, "%s/PS3_GAME/PIC1.PNG", path2);
				sprintf(string2, "%s/%s_320.PNG", cache_dir, list[*max ].title_id);
				if(stat(string2, &s3)<0) {cache_png(file, list[*max ].title_id);}

			}
		get_game_flags((*max));

		(*max) ++;
		continue;
		}
	else // check for PS2 games
		{
			list[*max ].split=0;
			sprintf(file, "%s/SYSTEM.CNF", path2);
			if(stat(file, &s)<0) sprintf(file, "%s/system.cnf", path2);

			if(stat(file, &s)>=0) { 
	
				list[*max ].flags=flag;
				sprintf(file, "[PS2] %s", entry->d_name);
				strncpy(list[*max ].title, file, 63);

				list[*max ].title[63]=0;
				list[*max ].cover=-1;

				sprintf(list[*max ].path, "%s", path2);
				sprintf(list[*max ].entry, "%s", entry->d_name);
				sprintf(list[*max ].content, "%s", "PS2");
				sprintf(list[*max ].title_id, "%s", "NO_ID");

				(*max) ++;
				continue;
			}
			else
			{

				sprintf(file, "%s/VIDEO_TS/VIDEO_TS.IFO", path2);

				if(display_mode!=1 && stat(file, &s)>=0) { 
					list[*max ].flags=flag;
					sprintf(file, "[DVD Video] %s", entry->d_name);
					strncpy(list[*max ].title, file, 63);

					list[*max ].title[63]=0;
					list[*max ].cover=-1;
					sprintf(file, "%s/VIDEO_TS", path2);
					sprintf(list[*max ].path, "%s", file);
					sprintf(list[*max ].entry, "%s", entry->d_name);
					sprintf(list[*max ].content, "%s", "DVD");
					sprintf(list[*max ].title_id, "%s", "NO_ID");

					(*max) ++;
	 				continue;
				}
				else //check for AVCHD on internal HDD
				{
					char ext_int[5]; ext_int[0]=0;
					if(strstr(path2, "dev_hdd0")!=NULL) sprintf(ext_int, "%s", "HDD ");

		sprintf(file, "%s/BDMV/INDEX.BDM", path2); // /dev_usb00x/AVCHD_something/BDMV/INDEX.BDM
		sprintf(BDtype,"AVCHD");
		if(stat(file, &s)<0) {
			sprintf(file, "%s/BDMV/index.bdmv", path2); // /dev_usb00x/something/BDMV/index.bdmv
			if(stat(file, &s)>=0) sprintf(BDtype,"BDMV");}

		if(display_mode!=1 && stat(file, &s)>=0)
			{

			char is_multiAVCHD[13];is_multiAVCHD[0]=0;
			sprintf(detailsfile, "%s/multiAVCHD.mpf", path2);
			if(stat(detailsfile, &s)>=0) sprintf(is_multiAVCHD,"%s", " (multiAVCHD)");
			is_multiAVCHD[13]=0;
			sprintf(path3, "[%sVideo] %s%s", ext_int, entry->d_name, is_multiAVCHD); path3[64]=0;
			sprintf(list[*max ].title, "%s", path3);

			list[*max ].flags=flag;
			list[*max ].title[63]=0;
			sprintf(list[*max ].title_id, "%s", "AVCHD");
			sprintf(list[*max ].path, "%s", path2);
			sprintf(list[*max ].entry, "%s", entry->d_name);
			sprintf(list[*max ].content, "%s", BDtype);
			list[*max ].cover=-1;

	sprintf(detailsfile, "%s/details.txt", path2);
	fp = fopen ( detailsfile, "r" );
	if ( fp != NULL ) 
	{
		fseek (fp, 0, SEEK_SET);
		char lines[2048]="/"; lines[1]=0;
		int cline=0;
		while (fscanf(fp,"%[^;];%[^;];%[^;];%[^;];%s\n", title, length, video, audio, web)==5)
		{ 
		
				cline++;
				if(expand_avchd==1) 
				{
					sprintf(string2, "[%sVideo] %s", ext_int, title); string2[63]=0;
					sprintf(list[*max].title, "%s", string2);
//					utf8_to_ansi(string2, list[*max].title, 63);
					list[*max].title[63]=0;

					list[*max ].flags=flag;
					sprintf(list[*max ].title_id, "%s", "AVCHD");
					sprintf(list[*max ].path, "%s", path2);
					sprintf(list[*max ].entry, "%s", entry->d_name);
					sprintf(list[*max ].content, "%s", BDtype);
					sprintf(list[*max ].details, "Duration: %s, Video: %s, Audio: %s", length, video, audio);
					list[*max ].cover=-1;
					(*max) ++;
				}
				else
				{
					if(cline==1) {
						is_multiAVCHD[13]=0;
						sprintf(string2, "[%sVideo] %s%s", ext_int, title, is_multiAVCHD); string2[58]=0;
						sprintf(list[*max].title, "%s", string2);
//						utf8_to_ansi(string2, list[*max].title, 58);list[*max].title[58]=0;
						}
					else
						sprintf(lines, "%s %s /", lines, title);
				}

				if(*max >=MAX_LIST) break;
		}
		lines[100]=0;
		if(expand_avchd==0) 
		{	lines[90]=0; sprintf(list[*max].details, "%s", lines);
			//utf8_to_ansi(lines, list[*max].details, 90);list[*max].details[90]=0;
		}
		else { if(cline>0) (*max) --;}
		fclose ( fp );
	}

			(*max) ++; 
			} // INDEX.BDM found



				}


			}


		}


/*
	dir2=opendir (path2);
	if(!dir2) continue;

	while(1)
		{ 
		if(display_mode==2) break;
		struct dirent *entry2=readdir (dir2);
		if(!entry2) break;
		if(entry2->d_name[0]=='.') continue;
		if(!(entry2->d_type & DT_DIR)) continue;

	sprintf(path3, "%s/%s", path2, entry2->d_name);
	dir3=opendir (path3);
	if(!dir3) continue;
	if(skip_entry==0) 
		{
	sprintf(string2, "Scanning, please wait!\n\n[%s]",path3);
	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	cellDbgFontPrintf( 0.3f, 0.45f, 0.8f, 0xc0c0c0c0, string2);
	set_texture( text_FMS, 1920, 48); display_img(0, 47, 1920, 60, 1920, 48, -0.15f, 1920, 48);	display_img(0, 952, 1920, 76, 1920, 48, -0.15f, 1920, 48);time ( &rawtime );	timeinfo = localtime ( &rawtime );	cellDbgFontPrintf( 0.83f, 0.89f, 0.7f ,0xc0a0a0a0, "%02d/%02d/%04d\n %02d:%02d:%02d ", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	set_texture( text_bmpIC, 320, 320);  display_img(800, 200, 320, 176, 320, 176, 0.0f, 320, 320);

	if(first_launch) cellDbgFontPrintf( 0.30f, 0.80f, 1.0f, 0x90909090, multi_loading);
	cellDbgFontDrawGcm();	flip(); }
	skip_entry++; if(skip_entry>8) skip_entry=0;
	while(1)
		{
		struct dirent *entry3=readdir (dir3);
		if(!entry3) break;
		if(entry3->d_name[0]=='.') continue;
		if(!(entry3->d_type & DT_DIR)) continue; 

		sprintf(file, "%s/PS3_GAME/USRDIR/EBOOT.BIN", path3); sprintf(list[*max ].path, "%s", path3);

/ *		if(stat(file, &s)<0) { 
			sprintf(file, "%s/%s/PS3_GAME/USRDIR/EBOOT.BIN", path3, entry3->d_name);
			sprintf(list[*max ].path, "%s/%s", path3, entry3->d_name);
		}
* /

		if(stat(file, &s)<0) continue;

		list[*max ].flags=flag;
		strncpy(list[*max ].title, entry3->d_name, 63);
		list[*max ].title[63]=0;
		list[*max ].cover=0;
		sprintf(list[*max ].entry, "%s", entry3->d_name);
		sprintf(list[*max ].content, "%s", "PS3");
		sprintf(list[*max ].title_id, "%s", "NO_ID");
			
		if(sel==0)
			{

			sprintf(file, "%s/PS3_GAME/PARAM.SFO", list[*max ].path);
//			if(stat(file, &s)<0) sprintf(file, "%s/PARAM.SFO", list[*max ].path);
			if(stat(file, &s)>=0) {
				parse_param_sfo(file, list[*max ].title+1*(list[*max ].title[0]=='_')); // move +1 with '_' 
				list[*max ].title[63]=0;
				parse_param_sfo_id(file, list[*max ].title_id);
				}
			}

		(*max) ++;
		if(*max >=MAX_LIST) break;
		}
		closedir(dir3);
	}
	closedir(dir2);
*/

		if(*max >=MAX_LIST) break;

		}
	
	closedir (dir);
    load_texture(text_bmpIC, blankBG, 320);
	is_game_loading=0;

}


/****************************************************/
/* FILE UTILS                                       */
/****************************************************/

//char string1[1024];

int copy_mode=0; // 0- normal 1-> pack files >= 4GB

int copy_is_split=0; // return 1 if files is split

//uint64_t global_device_bytes=0;


typedef struct _t_fast_files
{
	int64_t readed; // global bytes readed
	int64_t writed; // global bytes writed
	int64_t off_readed; // offset correction for bigfiles_mode == 2  (joining)
	int64_t len;    // global len of the file (value increased in the case of bigfiles_ mode == 2)

	int giga_counter; // counter for split files to 1GB for bigfiles_mode == 1 (split)
	u32 fl; // operation control
	int bigfile_mode;
	int pos_path; // filename position used in bigfiles

	char pathr[1024]; // read path 
	char pathw[1024]; // write path


	int use_doublebuffer; // if files >= 4MB use_doblebuffer =1;

	void *mem; // buffer for read/write files ( x2 if use_doublebuffer is fixed)
	int size_mem; // size of the buffer for read

	int number_frag; // used to count fragments files i bigfile_mode

	CellFsAio t_read;  // used for async read
	CellFsAio t_write; // used for async write

} t_fast_files __attribute__((aligned(8)));

t_fast_files *fast_files=NULL;

int fast_num_files=0;

int fast_used_mem=0;

int current_fast_file_r=0;
int current_fast_file_w=0;

int fast_read=0, fast_writing=0;

int files_opened=0;

int fast_copy_async(char *pathr, char *pathw, int enable)
{

	fast_num_files=0;

	fast_read=0;
	fast_writing=0;

	fast_used_mem=0;
	files_opened=0;

	current_fast_file_r= current_fast_file_w= 0;

	if(enable)
		{
		if(cellFsAioInit(pathr)!=CELL_FS_SUCCEEDED)  return -1;
		if(cellFsAioInit(pathw)!=CELL_FS_SUCCEEDED)  return -1;

		fast_files = (t_fast_files *) memalign(8, sizeof(t_fast_files)*MAX_FAST_FILES);
//		fast_files = (t_fast_files *) fast_files_mem;
		if(!fast_files) return -2;
		return 0;
		}
	else
		{
		if(fast_files) free(fast_files); fast_files=NULL;
		cellFsAioFinish(pathr);
		cellFsAioFinish(pathw);
		}

return 0;

}


int fast_copy_process();

int fast_copy_add(char *pathr, char *pathw, char *file)
{
	int size_mem;

	int strl= strlen(file);

	struct stat s;
				
				
	if(fast_num_files>=MAX_FAST_FILES || fast_used_mem>=0x2000000)//1000000)//C00000)//800000)
	{
	int ret=fast_copy_process();

		if(ret<0 || abort_copy) return ret;

	}

	if(fast_num_files>= MAX_FAST_FILES) {return -1;}
	
	fast_files[fast_num_files].bigfile_mode=0;

	if(strl>6)// && strstr(pathw, "/dev_hdd0")!=NULL)
		{
		char *p= file;
		p+= strl-6; // adjust for .666xx .x.part
		if(p[0]== '.' && p[1]== '6' && p[2]== '6' && p[3]== '6')
			{  
			if(p[4]!='0' ||  p[5]!='0')  {return 0;} // ignore this file
   			fast_files[fast_num_files].bigfile_mode=2; // joining split files
			}
/*		else if(strl>7)
			{

			p+= strl-7; // adjust for .666xx .x.part
			if(p[0]== '.' && p[1]!='1' && p[2]== '.' && p[3]== 'p' && p[4]== 'a' && p[5]== 'r' && p[6]== 't')
	   			fast_files[fast_num_files].bigfile_mode=2; // joining split files
			else return 0; // ignore

			} */
				
		}
	sprintf(fast_files[fast_num_files].pathr, "%s/%s", pathr, file);

	if(stat(fast_files[fast_num_files].pathr, &s)<0) {abort_copy=1;return -1;}

	sprintf(fast_files[fast_num_files].pathw, "%s/%s", pathw, file);

	// zero files
	if((int64_t) s.st_size==0LL)
		{
		int fdw;

		if(cellFsOpen(fast_files[fast_num_files].pathw, CELL_FS_O_CREAT | CELL_FS_O_TRUNC | CELL_FS_O_WRONLY, &fdw, 0,0)!=CELL_FS_SUCCEEDED)
			{
			DPrintf("Error Opening (write):\n%s\n\n", fast_files[current_fast_file_r].pathw);
			abort_copy=1;
			return -1;
			}
		cellFsClose(fdw);

		cellFsChmod(fast_files[fast_num_files].pathw, CELL_FS_S_IFMT | 0777);
		DPrintf("Copying:\n%s\nwWritten: 0 B\n", fast_files[current_fast_file_r].pathr);
		file_counter++;
		return 0;
		}

	if(fast_files[fast_num_files].bigfile_mode==2)
		{
/*		if(fast_files[fast_num_files].pathw[strlen(fast_files[fast_num_files].pathw)-1]=='t')
			{
			fast_files[fast_num_files].pathw[strlen(fast_files[fast_num_files].pathw)-7]=0; // truncate the .x.part extension
			fast_files[fast_num_files].pos_path=strlen(fast_files[fast_num_files].pathr)-7;
			}
		else
			{ */
			fast_files[fast_num_files].pathw[strlen(fast_files[fast_num_files].pathw)-6]=0; // truncate the .666xx extension
			fast_files[fast_num_files].pos_path=strlen(fast_files[fast_num_files].pathr)-6;
//			}
		fast_files[fast_num_files].pathr[fast_files[fast_num_files].pos_path]=0; // truncate the extension
		}
	
	if(copy_mode==1)
		{
		if(((uint64_t) s.st_size)>= 0x100000000ULL)
			{
			fast_files[fast_num_files].bigfile_mode=1;
			fast_files[fast_num_files].pos_path= strlen(fast_files[fast_num_files].pathw);
			fast_files[fast_num_files].giga_counter=0;
			
			copy_is_split=1;
			}
		
		}
	

	fast_files[fast_num_files].number_frag=0;
	fast_files[fast_num_files].fl=1;

	fast_files[fast_num_files].len= (int64_t) s.st_size;
	fast_files[fast_num_files].use_doublebuffer=0;
	fast_files[fast_num_files].readed= 0LL;
	fast_files[fast_num_files].writed= 0LL;

	fast_files[fast_num_files].t_read.fd= -1;
	fast_files[fast_num_files].t_write.fd= -1;

	if(((uint64_t) s.st_size)>=MAX_FAST_FILE_SIZE)
		{
		size_mem= MAX_FAST_FILE_SIZE;
		fast_files[fast_num_files].use_doublebuffer=1;
		}
	else size_mem= ((int) s.st_size);

	fast_files[fast_num_files].mem = memalign(32, size_mem + size_mem*(fast_files[fast_num_files].use_doublebuffer!=0)+1024);
	fast_files[fast_num_files].size_mem = size_mem;

	if(!fast_files[fast_num_files].mem) {abort_copy=1;return -1;}

	fast_used_mem+= size_mem;

	fast_num_files++;

	return 0;
}

void fast_func_read(CellFsAio *xaio, CellFsErrno error, int , uint64_t size)
{
	t_fast_files* fi = (t_fast_files *) xaio->user_data;

	if(error!=0 || size!= xaio->size)
		{
		fi->readed=-1;return;
		}
    else
		fi->readed+=(int64_t) size;

	fast_read=0;fi->fl=3;
	
}

void fast_func_write(CellFsAio *xaio, CellFsErrno error, int , uint64_t size)
{
	t_fast_files* fi = (t_fast_files *) xaio->user_data;

	if(error!=0 || size!= xaio->size)
		{
		fi->writed=-1;
		}
	else
		{

		fi->writed+=(int64_t) size;
		fi->giga_counter+= (int) size;
		global_device_bytes+=(int64_t) size;
		}

	fast_writing=2;
}

int fast_copy_process()
{

	int n;
	int seconds2= (int) time(NULL);	
	int fdr, fdw;
	char string1[1024];

	static int id_r=-1, id_w=-1;

	int error=0;

	int i_reading=0;

	int64_t write_end=0, write_size=0;

	while(current_fast_file_w<fast_num_files || fast_writing)
	{

		if(abort_copy) break;

		
		// open read
		if(current_fast_file_r<fast_num_files && fast_files[current_fast_file_r].fl==1 && !i_reading && !fast_read)
			{
			
				fast_files[current_fast_file_r].readed= 0LL;
				fast_files[current_fast_file_r].writed= 0LL;
				fast_files[current_fast_file_r].off_readed= 0LL;

				fast_files[current_fast_file_r].t_read.fd= -1;
				fast_files[current_fast_file_r].t_write.fd= -1;
				
				if(fast_files[current_fast_file_r].bigfile_mode==1)
					{
					DPrintf("Split file >= 4GB\n %s\n", fast_files[current_fast_file_r].pathr);
					sprintf(&fast_files[current_fast_file_r].pathw[fast_files[current_fast_file_r].pos_path],".666%2.2i",
						fast_files[current_fast_file_r].number_frag);
					}
				
				if(fast_files[current_fast_file_r].bigfile_mode==2)
					{
					DPrintf("Joining file >= 4GB\n %s\n", fast_files[current_fast_file_r].pathw);
					sprintf(&fast_files[current_fast_file_r].pathr[fast_files[current_fast_file_r].pos_path],".666%2.2i",
						fast_files[current_fast_file_r].number_frag);
					}
			

					
				if(cellFsOpen(fast_files[current_fast_file_r].pathr, CELL_FS_O_RDONLY, &fdr, 0,0)!=CELL_FS_SUCCEEDED) 
					{
					DPrintf("Error Opening (read):\n%s\n\n", fast_files[current_fast_file_r].pathr);
					error=-1;
					break;
					}else files_opened++;
				if(cellFsOpen(fast_files[current_fast_file_r].pathw, CELL_FS_O_CREAT | CELL_FS_O_TRUNC | CELL_FS_O_WRONLY, &fdw, 0,0)!=CELL_FS_SUCCEEDED)
					{
					DPrintf("Error Opening (write):\n%s\n\n", fast_files[current_fast_file_r].pathw);
					error=-2;
					break;
					}else files_opened++;

				if(fast_files[current_fast_file_r].bigfile_mode==0)
					{ DPrintf("Copying %s\n", fast_files[current_fast_file_r].pathr); file_counter++;}
				if(fast_files[current_fast_file_r].bigfile_mode)
					{ DPrintf("    -> Split part #%i\n", fast_files[current_fast_file_r].number_frag);}
//file_counter++;
				fast_files[current_fast_file_r].t_read.fd= fdr;

				fast_files[current_fast_file_r].t_read.offset= 0LL;
				fast_files[current_fast_file_r].t_read.buf= fast_files[current_fast_file_r].mem;
			
				fast_files[current_fast_file_r].t_read.size=fast_files[current_fast_file_r].len-fast_files[current_fast_file_r].readed;
				if((int64_t) fast_files[current_fast_file_r].t_read.size> fast_files[current_fast_file_r].size_mem)
					fast_files[current_fast_file_r].t_read.size=fast_files[current_fast_file_r].size_mem;

				fast_files[current_fast_file_r].t_read.user_data= (uint64_t )&fast_files[current_fast_file_r];

				fast_files[current_fast_file_r].t_write.fd= fdw;
				fast_files[current_fast_file_r].t_write.user_data= (uint64_t )&fast_files[current_fast_file_r];
				fast_files[current_fast_file_r].t_write.offset= 0LL;
				if(fast_files[current_fast_file_r].use_doublebuffer)
					fast_files[current_fast_file_r].t_write.buf= ((char *) fast_files[current_fast_file_r].mem) + fast_files[current_fast_file_r].size_mem;
				else
					fast_files[current_fast_file_r].t_write.buf= fast_files[current_fast_file_r].mem;

				fast_read=1;fast_files[current_fast_file_r].fl=2;
				if(cellFsAioRead(&fast_files[current_fast_file_r].t_read, &id_r, fast_func_read)!=0)
					{
					id_r=-1;
					error=-3;
					DPrintf("Fail to perform Async Read\n\n");
					fast_read=0;
					break;
					}

				i_reading=1;

			}

		// fast read end

		if(current_fast_file_r<fast_num_files && fast_files[current_fast_file_r].fl==3 && !fast_writing)
			{
			id_r=-1;
			//fast_read=0;

			if(fast_files[current_fast_file_r].readed<0LL)
				{
					DPrintf("Error Reading %s\n", fast_files[current_fast_file_r].pathr);
					error=-3;
					break;
				}
            
			// double buffer

			if(fast_files[current_fast_file_r].use_doublebuffer)
				{
					//DPrintf("Double Buff Write\n");
					
					current_fast_file_w=current_fast_file_r;
					
					memcpy(((char *) fast_files[current_fast_file_r].mem)+fast_files[current_fast_file_r].size_mem,
					fast_files[current_fast_file_r].mem, fast_files[current_fast_file_r].size_mem);

					fast_files[current_fast_file_w].t_write.size= fast_files[current_fast_file_r].t_read.size;
					
					if(fast_files[current_fast_file_w].bigfile_mode==1)
						fast_files[current_fast_file_w].t_write.offset= (int64_t) fast_files[current_fast_file_w].giga_counter;
					else
						fast_files[current_fast_file_w].t_write.offset= fast_files[current_fast_file_w].writed;
					
					fast_writing=1;



					if(cellFsAioWrite(&fast_files[current_fast_file_w].t_write, &id_w, fast_func_write)!=0)
						{
						id_w=-1;
						error=-4;
						DPrintf("Fail to perform Async Write\n\n");
						fast_writing=0;
						break;
						}

					if(fast_files[current_fast_file_r].readed<fast_files[current_fast_file_r].len)
						{
						fast_files[current_fast_file_r].t_read.size=fast_files[current_fast_file_r].len-fast_files[current_fast_file_r].readed;
						if((int64_t) fast_files[current_fast_file_r].t_read.size> fast_files[current_fast_file_r].size_mem)
							fast_files[current_fast_file_r].t_read.size=fast_files[current_fast_file_r].size_mem;

						fast_files[current_fast_file_r].fl=2;
						fast_files[current_fast_file_r].t_read.offset= fast_files[current_fast_file_r].readed-fast_files[current_fast_file_r].off_readed;

						fast_read=1;
						if(cellFsAioRead(&fast_files[current_fast_file_r].t_read, &id_r, fast_func_read)!=0)
							{
							id_r=-1;
							error=-3;
							DPrintf("Fail to perform Async Read\n\n");
							fast_read=0;
							break;
							}
						}
					else 
						{
						if(fast_files[current_fast_file_r].bigfile_mode==2)
							{
							struct stat s;
							
							fast_files[current_fast_file_r].number_frag++;

							fast_files[current_fast_file_r].off_readed= fast_files[current_fast_file_r].readed;

							DPrintf("    -> .666%2.2i\n", fast_files[current_fast_file_r].number_frag);
							sprintf(&fast_files[current_fast_file_r].pathr[fast_files[current_fast_file_r].pos_path],".666%2.2i",
								fast_files[current_fast_file_r].number_frag);
/*							if(stat(fast_files[current_fast_file_r].pathr, &s)<0)
								sprintf(&fast_files[current_fast_file_r].pathr[fast_files[current_fast_file_r].pos_path],".%i.part",
								fast_files[current_fast_file_r].number_frag); */

							if(stat(fast_files[current_fast_file_r].pathr, &s)<0) {current_fast_file_r++;i_reading=0;}
							else
								{
								if(fast_files[current_fast_file_r].t_read.fd>=0) 
									{cellFsClose(fast_files[current_fast_file_r].t_read.fd);files_opened--;}fast_files[current_fast_file_r].t_read.fd=-1;
								
								if(cellFsOpen(fast_files[current_fast_file_r].pathr, CELL_FS_O_RDONLY, &fdr, 0,0)!=CELL_FS_SUCCEEDED) 
									{
									DPrintf("Error Opening (read):\n%s\n\n", fast_files[current_fast_file_r].pathr);
									error=-1;
									break;
									}else files_opened++;

								fast_files[current_fast_file_r].t_read.fd= fdr;

								fast_files[current_fast_file_r].len += (int64_t) s.st_size;

								fast_files[current_fast_file_r].t_read.offset= 0LL;
								fast_files[current_fast_file_r].t_read.buf= fast_files[current_fast_file_r].mem;
							

								fast_files[current_fast_file_r].t_read.size=fast_files[current_fast_file_r].len-fast_files[current_fast_file_r].readed;
								if((int64_t) fast_files[current_fast_file_r].t_read.size> fast_files[current_fast_file_r].size_mem)
									fast_files[current_fast_file_r].t_read.size=fast_files[current_fast_file_r].size_mem;

								fast_files[current_fast_file_r].t_read.user_data= (uint64_t )&fast_files[current_fast_file_r];

								fast_read=1;
								if(cellFsAioRead(&fast_files[current_fast_file_r].t_read, &id_r, fast_func_read)!=0)
									{
									id_r=-1;
									error=-3;
									DPrintf("Fail to perform Async Read\n\n");
									fast_read=0;
									break;
									}

								fast_files[current_fast_file_r].fl=2;

								}
							}
						else
							{fast_files[current_fast_file_r].fl=5;current_fast_file_r++;i_reading=0;}
						
						
						}

				}
			else
			// single buffer	
				{

					current_fast_file_w=current_fast_file_r;
					fast_files[current_fast_file_w].t_write.size= fast_files[current_fast_file_r].t_read.size;

					fast_files[current_fast_file_w].t_write.offset= fast_files[current_fast_file_w].writed;
					
					fast_writing=1;

					if(cellFsAioWrite(&fast_files[current_fast_file_w].t_write, &id_w, fast_func_write)!=0)
						{
						id_w=-1;
						error=-4;
						DPrintf("Fail to perform Async Write\n\n");
						fast_writing=0;
						break;
						}
					
					
					current_fast_file_r++;
					i_reading=0;
				}
			}
		
		// fast write end
		if(fast_writing>1)
			{
				fast_writing=0;
				id_w=-1;

				if(fast_files[current_fast_file_w].writed<0LL)
				{
					DPrintf("Error Writing %s\n", fast_files[current_fast_file_w].pathw);
					error=-4;
					break;
				}

				write_end=fast_files[current_fast_file_w].writed;
				write_size=fast_files[current_fast_file_w].len;

				if(fast_files[current_fast_file_w].writed>=fast_files[current_fast_file_w].len)
					{
				        if(fast_files[current_fast_file_w].t_read.fd>=0)
							{cellFsClose(fast_files[current_fast_file_w].t_read.fd);files_opened--;}fast_files[current_fast_file_w].t_read.fd=-1;
						if(fast_files[current_fast_file_w].t_write.fd>=0)
							{cellFsClose(fast_files[current_fast_file_w].t_write.fd);files_opened--;}fast_files[current_fast_file_w].t_write.fd=-1;
						
						cellFsChmod(fast_files[current_fast_file_w].pathw, CELL_FS_S_IFMT | 0777);

						if(fast_files[current_fast_file_w].bigfile_mode==1)
							{
							fast_files[current_fast_file_w].pathw[fast_files[current_fast_file_w].pos_path]=0;
							}


						fast_files[current_fast_file_w].fl=4; //end of proccess
						fast_files[current_fast_file_w].writed=-1LL;
						current_fast_file_w++;
						//if(current_fast_file_r<current_fast_file_w) current_fast_file_w=current_fast_file_r;
//file_counter++;
					}
				else
				// split big files
				if(fast_files[current_fast_file_w].bigfile_mode==1 && fast_files[current_fast_file_w].giga_counter>=0x40000000)
					{
						if(fast_files[current_fast_file_w].t_write.fd>=0)
							{cellFsClose(fast_files[current_fast_file_w].t_write.fd);files_opened--;}fast_files[current_fast_file_w].t_write.fd=-1;

						cellFsChmod(fast_files[current_fast_file_w].pathw, CELL_FS_S_IFMT | 0777);
						
						fast_files[current_fast_file_w].giga_counter=0;
						fast_files[current_fast_file_w].number_frag++;
						sprintf(&fast_files[current_fast_file_w].pathw[fast_files[current_fast_file_w].pos_path],".666%2.2i",
							fast_files[current_fast_file_w].number_frag);
						DPrintf("    -> .666%2.2i\n", fast_files[current_fast_file_w].number_frag);
						
						if(cellFsOpen(fast_files[current_fast_file_w].pathw, CELL_FS_O_CREAT | CELL_FS_O_TRUNC | CELL_FS_O_WRONLY, &fdw, 0,0)!=CELL_FS_SUCCEEDED)
							{
							DPrintf("Error Opening2 (write):\n%s\n\n", fast_files[current_fast_file_w].pathw);
							error=-2;
							break;
							}else files_opened++;

						fast_files[current_fast_file_w].t_write.fd=fdw;
					}

				
			}


	int seconds= (int) (time(NULL)-time_start);
	int eta=0;
	lastINC3=0;
	if(use_symlinks==1 || no_real_progress==1)
	{
		eta=(copy_file_counter-file_counter)/(file_counter/seconds);
		if( ( ((int)(file_counter*100ULL/copy_file_counter)) - lastINC2)>0)
		{
			lastINC2=(int) (file_counter*100ULL / copy_file_counter);
			if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}
//			if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
		}
	}
	else
	{
		eta=(copy_global_bytes-global_device_bytes)/(global_device_bytes/seconds);
		if( ( ((int)(global_device_bytes*100ULL/copy_global_bytes)) - lastINC2)>0)
		{
			lastINC2=(int) (global_device_bytes*100ULL / copy_global_bytes);
			if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}
//			if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
		}
	}

//if(lastINC2>98) cellMsgDialogAbort();

//sprintf(string1,"%1.3f of %1.3f GB copied (elapsed: %2.2i:%2.2i:%2.2i)",((double) global_device_bytes)/(1024.0*1024.*1024.0),((double) copy_global_bytes)/(1024.0*1024.*1024.0), seconds/3600, (seconds/60) % 60, seconds % 60);

if(lastINC3>0 || (time(NULL)-seconds2)!=0 || use_symlinks==1) 
{
	
	if(join_copy==1)
		sprintf(string1,"Installed %.0f of %.0f MB. Remaining: %i:%2.2i min", ((double) global_device_bytes)/(1024.0*1024.0),((double) copy_global_bytes)/(1024.0*1024.0), (eta/60), eta % 60);

	else
	{
		if(use_symlinks==1)
		{
			if(no_real_progress==1)
				sprintf(string1,"Files linked: %i. Elapsed time: %i:%2.2i min", file_counter, (seconds/60), seconds % 60);
			else
				sprintf(string1,"Files linked: %i/%i. Remaining: %i:%2.2i min", file_counter, copy_file_counter, (eta/60), eta % 60);
		}
		else
		{
			if(no_real_progress==1)
				sprintf(string1,"Copied %.0f MB (%i of %i files). Elapsed %2.2i:%2.2i min",((double) global_device_bytes)/(1024.0*1024.0), file_counter+1, copy_file_counter, (seconds/60), seconds % 60);
			else
				sprintf(string1,"Copied %.0f / %.0f MB (%i/%i) Remaining: %i:%2.2i min", ((double) global_device_bytes)/(1024.0*1024.0),((double) copy_global_bytes)/(1024.0*1024.0), file_counter+1, copy_file_counter, (eta/60), eta % 60);
		}
	}
	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x101010ff);
	cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
	cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Press /\\ to abort");
	if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
 	cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, string1);
	cellDbgFontDrawGcm();
	seconds2= (int) (time(NULL));
	flip(); 
}

	pad_read();
	if ( new_pad & BUTTON_TRIANGLE ) 
		{
		abort_copy=1;
		DPrintf("Copy process aborted by user. \n");
		error=-666;
		break;
		}
	
	}

	if(error && error!=-666)
		{
		DPrintf("Error!\nFiles Opened %i\n Waiting 2 seconds to display fatal error\n", files_opened);
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
//		draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x200020ff);
		cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xffffffff, string1);
		cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xffffffff, "Press /\\ to abort");
		cellDbgFontDrawGcm();
		flip();
		sys_timer_usleep(2*1000000);
		}


	if(fast_writing==1 && id_w>=0)
		{
		cellFsAioCancel(id_w);
		id_w=-1;
		sys_timer_usleep(200000);
		}
	
	fast_writing=0;

	if(fast_read==1 && id_r>=0)
		{
		cellFsAioCancel(id_r);
		id_r=-1;
		sys_timer_usleep(200000);
		}
	
	fast_read=0;

	for(n=0;n<fast_num_files;n++)
	{
		if(fast_files[n].t_read.fd>=0) 
			{
			
			cellFsClose(fast_files[n].t_read.fd);fast_files[n].t_read.fd=-1;
			files_opened--;
			}
		if(fast_files[n].t_write.fd>=0)	
			{
			
			cellFsClose(fast_files[n].t_write.fd);fast_files[n].t_write.fd=-1;
			files_opened--;
			}

		if(fast_files[n].mem) free(fast_files[n].mem); fast_files[n].mem=NULL;
	}

	fast_num_files=0;

	fast_writing=0;

	fast_used_mem=0;

	current_fast_file_r= current_fast_file_w= 0;

	if(error) abort_copy=666+100+error;
	return error;
}

void file_copy(char *path, char *path2, int progress)
{
		if((strstr(path, "/pvd_usb")!=NULL && !pfs_enabled) || (strstr(path2, "/pvd_usb")!=NULL)) return;

		if(progress){
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); flip();
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); flip();
		}
		dialog_ret=0;
		char rdr[255];
		time_start=time(NULL);
		int fs;
		int fd;
		uint64_t fsiz = 0;
		uint64_t msiz = 0;
		sprintf(rdr, "%s", path);
		int seconds2=0;
		char string1[1024];

#if (CELL_SDK_VERSION>0x210001)
		PFS_HFILE fdr = PFS_FILE_INVALID;
		if(strstr(path, "/pvd_usb")!=NULL)
		{
			if ((fdr = PfsFileOpen(path)) == PFS_FILE_INVALID)
				return;
			if (PfsFileGetSizeFromHandle(fdr, &msiz) != 0) {
				PfsFileClose(fdr);
				return;
			}
		}
		else
#endif
		{
			cellFsOpen(path, CELL_FS_O_RDONLY, &fs, NULL, 0);
			cellFsLseek(fs, 0, CELL_FS_SEEK_END, &msiz);
			cellFsClose(fs);
		}

//		uint64_t chunk = 16*1024;
		uint64_t chunk = BUF_SIZE;
		if(msiz<chunk && msiz>0) chunk=msiz;

//		char w[chunk];
		lastINC2=0;lastINC=0;
		uint64_t written=0;
		remove(path2); abort_copy=0;


#if (CELL_SDK_VERSION>0x210001)
		fdr = PFS_FILE_INVALID;
		uint32_t size;
		if (strstr(path, "/pvd_usb")!=NULL)
		{
			if((fdr = PfsFileOpen(path)) == PFS_FILE_INVALID) {return;}
		}
		else
#endif
			cellFsOpen(rdr, CELL_FS_O_RDONLY, &fs, NULL, 0);

		copy_file_counter=1; 
		copy_global_bytes=msiz;
		lastINC=0; lastINC3=0; lastINC2=0;

		sprintf(rdr, "%s", path2);
		cellFsOpen(rdr, CELL_FS_O_CREAT|CELL_FS_O_RDWR|CELL_FS_O_APPEND, &fd, NULL, 0);
		while(fsiz < msiz && abort_copy==0)
		{		
			//if(to_reboot) {abort_copy=1; break;}

			if((fsiz+chunk) > msiz)
			{
				chunk = (msiz-fsiz);
#if (CELL_SDK_VERSION>0x210001)
				if (strstr(path, "/pvd_usb")!=NULL)
				{
					if(PfsFileRead(fdr, buf, chunk, &size) != 0) {abort_copy=1;break;}
				}
				else
#endif
				{
					if(cellFsRead(fs, (void *)buf, chunk, NULL)!=CELL_FS_SUCCEEDED)	{abort_copy=1;break;}
				}

				{
					if(cellFsWrite(fd, (const void *)buf, chunk, &written)!=CELL_FS_SUCCEEDED){abort_copy=1;break;};
					if(written!=chunk){abort_copy=1;break;}
					global_device_bytes+=chunk;
					break;
				}
			}
			else
			{
#if (CELL_SDK_VERSION>0x210001)
				if (strstr(path, "/pvd_usb")!=NULL)
				{
					if(PfsFileRead(fdr, buf, chunk, &size) != 0) {abort_copy=1;break;}
				}
				else
#endif
				{
					if(cellFsRead(fs, (void *)buf, chunk, NULL)!=CELL_FS_SUCCEEDED){abort_copy=1;break;}
				}
				if(cellFsWrite(fd, (const void *)buf, chunk, &written)!=CELL_FS_SUCCEEDED){abort_copy=1;break;}
				if(written!=chunk){abort_copy=1;break;}
				fsiz = fsiz + chunk;
				global_device_bytes=fsiz;
			}

			int seconds= (int) (time(NULL)-time_start);

	lastINC3=0;
	int eta=(copy_global_bytes-global_device_bytes)/(global_device_bytes/seconds);
if( ( ( ((int)(global_device_bytes*100ULL/copy_global_bytes)) - lastINC2)>0 || (time(NULL)-seconds2)>0) && progress!=0)
{
//	sprintf(string1,"%1.3f of %1.3f MB copied (elapsed: %2.2i:%2.2i:%2.2i) Remaining: %imin %2.2isec",((double) global_device_bytes)/(1024.0*1024.0),((double) copy_global_bytes)/(1024.0*1024.0), seconds/3600, (seconds/60) % 60, seconds % 60, (eta/60), eta % 60);

	sprintf(string1,"Copied %1.2f of %1.2f MB. Remaining: %i:%2.2i min",((double) global_device_bytes)/(1024.0*1024.0),((double) copy_global_bytes)/(1024.0*1024.0), (eta/60), eta % 60);


	cellMsgDialogProgressBarSetMsg(
	CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,
	string1);

	lastINC2=(int) (global_device_bytes*100ULL / copy_global_bytes);
	if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}

	if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
	seconds2= (int) (time(NULL));
	flip();

}
	//cellSysutilCheckCallback();
	pad_read();
	if ( old_pad & BUTTON_TRIANGLE || new_pad & BUTTON_CIRCLE || dialog_ret==3) {abort_copy=1; new_pad=0; old_pad=0; break;}


		}


		cellFsClose(fd);
#if (CELL_SDK_VERSION>0x210001)
		if(strstr(path, "/pvd_usb")==NULL) cellFsClose(fs); else PfsFileClose(fdr);
#else
		cellFsClose(fs);
#endif
		cellFsChmod(rdr, 0666);
		if( global_device_bytes != copy_global_bytes) abort_copy=1;
		if(abort_copy==1) remove(path2);
		if(progress!=0){
		cellMsgDialogAbort();sys_timer_usleep(100000); flip();
		}
}


void write_last_play( const char *gamebin, const char *path, const char *tname, const char *tid, int dboot)
{
	(void) tid;
	(void) tname;

	if(stat(last_play_dir, &s3)<0) return;

	dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Setting data for last played game, please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
	flipc(60);

	char org_param_sfo[512], sldir[512];//, dldir[512];
	char PIC1[512], PIC0[512], ICON0[512], ICON1_PAM[512], EBOOT[512];;
	char _PIC1[512], _PIC0[512], _ICON0[512], _ICON1_PAM[512], _EBOOT[512];

	if(strstr( gamebin, "/PS3_GAME/")!=NULL){

		sprintf( org_param_sfo, "%s/PS3_GAME/PARAM.SFO", path);
		sprintf( EBOOT, "%s/PS3_GAME/USRDIR/EBOOT.BIN", path);
	//	PIC1.PNG PIC0.PNG ICON0.PNG ICON1.PAM
		sprintf( PIC0, "%s/PS3_GAME/PIC0.PNG", path);
		sprintf( PIC1, "%s/PS3_GAME/PIC1.PNG", path);
		sprintf( ICON0, "%s/PS3_GAME/ICON0.PNG", path);
		sprintf( ICON1_PAM, "%s/PS3_GAME/ICON1.PAM", path);
		sprintf (sldir, "%s/PS3_GAME/USRDIR", path);
	}
	else
	{
		sprintf( org_param_sfo, "%s/PARAM.SFO", path);
		sprintf( EBOOT, "%s/USRDIR/EBOOT.BIN", path);
		sprintf( PIC0, "%s/PIC0.PNG", path);
		sprintf( PIC1, "%s/PIC1.PNG", path);
		sprintf( ICON0, "%s/ICON0.PNG", path);
		sprintf( ICON1_PAM, "%s/ICON1.PAM", path);
		sprintf (sldir, "%s/USRDIR", path);
	}

/*
	if(strstr(path, "/dev_hdd0")!=NULL && (c_firmware>3.54f || payload==0)){ //create shadow copy
		sprintf (dldir, "%s/USRDIR", last_play_dir);
		use_symlinks=1;
		my_game_copy(sldir, dldir);
	}
*/

	sprintf( _PIC0, "%s/PIC0.PNG", last_play_dir);
	sprintf( _PIC1, "%s/PIC1.PNG", last_play_dir);
	sprintf( _ICON0, "%s/ICON0.PNG", last_play_dir);
	sprintf( _EBOOT, "%s/USRDIR/MM_EBOOT.BIN", last_play_dir);
	sprintf( _ICON1_PAM, "%s/ICON1.PAM", last_play_dir);

	if(stat(PIC0, &s3)>=0) file_copy( PIC0, _PIC0, 0); else remove(_PIC0);
	if(stat(PIC1, &s3)>=0) file_copy( PIC1, _PIC1, 0); else remove(_PIC1);
	if(stat(EBOOT, &s3)>=0) file_copy( EBOOT, _EBOOT, 0);
	if(stat(ICON0, &s3)>=0) file_copy( ICON0, _ICON0, 0);	flip();
	if(stat(ICON1_PAM, &s3)>=0) file_copy( ICON1_PAM, _ICON1_PAM, 0); else remove(_ICON1_PAM);

	char LASTGAME[512], SELF_NAME[512], SELF_PATH[512], SELF_USBP[16], SELF_BOOT[8];
	sprintf(LASTGAME, "%s/LASTPLAY.BIN", last_play_dir);
	sprintf(SELF_NAME, "SELF=%s", gamebin);
	sprintf(SELF_PATH, "PATH=%s", path);
	sprintf(SELF_USBP, "USBP=%i", patchmode);
	if(c_firmware==3.55f)
		sprintf(SELF_BOOT, "BOOT=%i", dboot+2);
	else
		sprintf(SELF_BOOT, "BOOT=%i", dboot);

	char CrLf[2]; CrLf [0]=13; CrLf [1]=10; CrLf[2]=0;
	FILE *fpA;
	remove(LASTGAME);
	fpA = fopen ( LASTGAME, "w" );
	fputs (SELF_NAME,  fpA );fputs ( CrLf,  fpA );
	fputs (SELF_PATH,  fpA );fputs ( CrLf,  fpA );
	fputs (SELF_USBP,  fpA );fputs ( CrLf,  fpA );
	fputs (SELF_BOOT,  fpA );fputs ( CrLf,  fpA );
	fclose(fpA);
	flip();

//	change_param_sfo_field( last_play_sfo, (char*)"TITLE", tname);	flip();
//	change_param_sfo_field( last_play_sfo, (char*)"TITLE_ID", tid);	flip();
	file_copy( org_param_sfo, last_play_sfo, 0);
	change_param_sfo_field( last_play_sfo, (char*)"CATEGORY", (char*)"HG");
	cellMsgDialogAbort();
}


void cache_png(char *path, char *title_id)
{
		char src[512], dst[255], tmp1[512], tmp2[512];


		if(strstr (title_id, "AVCHD")!=NULL || strstr (title_id,"NO_ID")!=NULL) sprintf(src, "%s/BOOT.PNG", app_usrdir); else sprintf(src, "%s", path);
		mkdir(cache_dir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

		FILE *fpA; char raw_texture[512];
		sprintf(raw_texture, "%s/%s_320.PNG", cache_dir, title_id);
		if(stat(raw_texture, &s3)>=0) return;

		if(!(strstr (title_id, "AVCHD")!=NULL || strstr (title_id,"NO_ID")!=NULL))
		{
			sprintf(tmp1, "%s", path);
			tmp1[strlen(tmp1)-9]=0;
			sprintf(tmp2, "%s/ICON0.PNG", tmp1);
			sprintf(dst, "%s/%s_320.PNG", cache_dir, title_id);
			file_copy((char *)tmp2, (char*)dst, 0);
		}

		memset(text_bmp, 0x50, 1920*1080*4);

		if(strstr(path, "/pvd_usb")!=NULL)
		{
			sprintf(dst, "%s/%s_1920.PNG", cache_dir, title_id);
			file_copy((char *)src, (char*)dst, 0);
			if(strstr(src, "/PIC1.PNG")!=NULL && stat(dst, &s3)<0)
			{
				src[strlen(src)-5]=0x30;
				file_copy((char *)src, (char*)dst, 0);
			}

			if(stat(dst, &s3)>=0 && strstr(src, "/PIC0.PNG")!=NULL)
			{
				load_texture( text_FONT, dst, 1000);
				put_texture( text_bmp, text_FONT, 1000, 560, 1000, 460, 260, 0, 0);
			}
			else
				load_texture( text_bmp, dst, 1920);

			//remove(dst);
		}
		else
		{
			if(strstr(src, "/PIC1.PNG")!=NULL && stat(src, &s3)<0)
			{

				src[strlen(src)-5]=0x30;
				if(stat(src, &s3)>=0)
				{
					load_texture( text_FONT, src, 1000);
					put_texture( text_bmp, text_FONT, 1000, 560, 1000, 460, 260, 0, 0);
				}
				else
				{
					src[strlen(src)-5]=0x32;
					if(stat(src, &s3)>=0)
					{
						load_texture( text_bmp, src, 310);
						mip_texture( text_FONT, text_bmp, 310, 250, 2); //scale to 620x500
						memset(text_bmp, 0x00, 1920*1080*4);
						put_texture( text_bmp, text_FONT, 620, 500, 620, 650, 290, 0, 0);
					}
					else
					{
//						goto just_leave;
						sprintf(src, "%s/AVCHD.JPG", app_usrdir);
						load_texture( text_bmp, src, 1920);
					}
				}

			}
			else
			{
				if(stat(src, &s3)>=0) { 
					sprintf(dst, "%s/%s_1920.PNG", cache_dir, title_id);
					file_copy((char *)src, (char*)dst, 0); 
					load_texture( text_bmp, dst, 1920);
				}
				else memset(text_bmp, 0x50, 1920*1080*4);
				src[strlen(src)-5]=0x30;
				if(stat(src, &s3)>=0)
				{
//					use_png_alpha=1;
					load_texture( text_FONT, src, 1000);
					put_texture_with_alpha( text_bmp, text_FONT, 1000, 560, 1000, 640, 380, 0, 0);
				}



			}
		}

		sprintf(raw_texture, "%s/%s_960.RAW", cache_dir, title_id);
		remove(raw_texture);
/*		if(stat(raw_texture, &s3)<0)
		{
			blur_texture(text_bmp, 1920, 1080, 0, 0, 1920, 1080,  0, 0, 1, 1);
			mip_texture( text_FONT, text_bmp, 1920, 1080, -2); //scale to 960x540
			blur_texture(text_FONT, 960, 540, 0, 0, 960, 540,  90, 0, 1, 1);
			fpA = fopen ( raw_texture, "wb" );
			fwrite(text_FONT, (960*540*4), 1, fpA);
			fclose(fpA);
		}
*/

		sprintf(raw_texture, "%s/%s_640.RAW", cache_dir, title_id);
		if(stat(raw_texture, &s3)<0)
		{
		//remove(raw_texture);
			mip_texture( text_FONT, text_bmp, 1920, 1080, -3); //scale to 640x360
			fpA = fopen ( raw_texture, "wb" );
			fwrite(text_FONT, (640*360*4), 1, fpA);
			fclose(fpA);
		}
/*
		sprintf(raw_texture, "%s/%s_480.RAW", cache_dir, title_id);
		remove(raw_texture);
		mip_texture( text_FONT, text_bmp, 1920, 1080, -4); //scale to 480x270
		fpA = fopen ( raw_texture, "wb" );
		fwrite(text_FONT, (480*270*4), 1, fpA);
		fclose(fpA);
*/
		sprintf(raw_texture, "%s/%s_320.RAW", cache_dir, title_id);
		if(stat(raw_texture, &s3)<0)
		{
		//remove(raw_texture);
			mip_texture( text_FONT, text_bmp, 1920, 1080, -6); //scale to 320x180
			fpA = fopen ( raw_texture, "wb" );
			fwrite(text_FONT, (320*180*4), 1, fpA);
			fclose(fpA);
		}

		sprintf(raw_texture, "%s/%s_240.RAW", cache_dir, title_id);
		if(stat(raw_texture, &s3)<0)
		{
			//remove(raw_texture);
			mip_texture( text_FONT, text_bmp, 1920, 1080, -8); //scale to 240x135
			fpA = fopen ( raw_texture, "wb" );
			fwrite(text_FONT, (240*135*4), 1, fpA);
			fclose(fpA);
		}

		sprintf(raw_texture, "%s/%s_160.RAW", cache_dir, title_id);
		if(stat(raw_texture, &s3)<0)
		{
			//remove(raw_texture);
			mip_texture( text_FONT, text_bmp, 1920, 1080, -12); //scale to 160x90
			fpA = fopen ( raw_texture, "wb" );
			fwrite(text_FONT, (160*90*4), 1, fpA);
			fclose(fpA);
		}

		sprintf(raw_texture, "%s/%s_80.RAW", cache_dir, title_id);
		if(stat(raw_texture, &s3)<0)
		{
		//remove(raw_texture);
			mip_texture( text_FONT, text_bmp, 1920, 1080, -24); //scale to 80x45
			fpA = fopen ( raw_texture, "wb" );
			fwrite(text_FONT, (80*45*4), 1, fpA);
			fclose(fpA);
		}

//just_leave:
		return;
}

static void del_temp(char *path)
{
	DIR  *dir;
	char tr[512];
	dir=opendir (path);
	if(!dir) return;

	while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;

		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;

		if(!(entry->d_type & DT_DIR))
			{
			sprintf(tr, "%s/%s", path, entry->d_name);
			remove(tr);
			}
		}

	closedir(dir);
	return;

}


#if (CELL_SDK_VERSION>0x210001)
static int my_game_test_pfsm(char *path, int to_abort)
{
	PFS_HFIND dir;
	PFS_FIND_DATA entry;

	dir = PfsFileFindFirst(path, &entry);
	if (!dir)
		return -1;

	do {
		if (!strcmp(entry.FileName, ".") ||
			!strcmp(entry.FileName, ".."))
			continue;

		if ((entry.FileAttributes & PFS_FIND_DIR)) {
			char *d1f= (char *) malloc(512);

			if (!d1f) {
				free(d1f);
				PfsFileFindClose(dir);
				DPrintf("malloc() Error!!!\n\n");
				abort_copy = 2;
				return -1;
			}
			
			sprintf(d1f, "%s/%s", path, entry.FileName);
			num_directories++;
			my_game_test_pfsm(d1f, to_abort);
			free(d1f);

		} else {
			PFS_HFILE fdr;
			uint64_t write_size;

			sprintf(d1, "%s/%s", path, entry.FileName);

			if(to_abort!=2){

				if ((fdr = PfsFileOpen(d1)) == PFS_FILE_INVALID) {
					DPrintf("Error Opening (read):\n%s\n\n", d1);
					abort_copy = 1;
					PfsFileFindClose(dir);
					return -1;
				}

				PfsFileGetSizeFromHandle(fdr, &write_size);
				if(write_size>=0x100000000LL)	{num_files_big++;}
				global_device_bytes += write_size;
				PfsFileClose(fdr);
			}

			file_counter++;			

			int seconds= (int) (time(NULL)-time_start);
			if((seconds>10) && to_abort==1) {abort_copy=1; break;}//if(f) free(f); //file_counter>4000 || 

				pad_read();
				if (new_pad & BUTTON_TRIANGLE) {
					abort_copy = 1;
					DPrintf("Aborted by user \n");
					break;
				}

		}
		pad_read();
		if (new_pad & BUTTON_TRIANGLE) {
			abort_copy = 1;
			DPrintf("Aborted by user \n");
			break;
		}

	} while (PfsFileFindNext(dir, &entry) == 0);

	PfsFileFindClose(dir);
	return 0;
}

static int _my_game_copy_pfsm(char *path, char *path2)
{
	PFS_HFIND dir;
	PFS_FIND_DATA entry;
	int seconds2=0;

	dir = PfsFileFindFirst(path, &entry);
	if (!dir)
		return -1;

	do {
		if (!strcmp(entry.FileName, ".") ||
			!strcmp(entry.FileName, ".."))
			continue;

		if ((entry.FileAttributes & PFS_FIND_DIR)) {
			char *d1f= (char *) malloc(512);
			char *d2f= (char *) malloc(512);

			if (!d1f || !d2f) {
				if (d1f)
					free(d1f);
				if (d2f)
					free(d2f);
				PfsFileFindClose(dir);
				DPrintf("malloc() Error!!!\n\n");
				abort_copy = 2;
				return -1;
			}
			
			sprintf(d1f, "%s/%s", path, entry.FileName);
			sprintf(d2f, "%s/%s", path2, entry.FileName);
			mkdir(d2f, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
			_my_game_copy_pfsm(d1f, d2f);
			free(d1f);
			free(d2f);
		} else {
			PFS_HFILE fdr;
			uint64_t write_size, write_end = 0;
			uint32_t size;
			int fdw;

			char *f1 = (char *) malloc(1024);
			char *f2 = (char *) malloc(1024);

			if (!f1 || !f2) {
				if (f1)
					free(f1);
				if (f2)
					free(f2);
				DPrintf("malloc() Error!!!\n\n");
				abort_copy = 2;
				PfsFileFindClose(dir);
				return -1;
			}
			sprintf(f1, "%s/%s", path, entry.FileName);
			sprintf(f2, "%s/%s", path2, entry.FileName);
			char *string1 = (char *) malloc(1024);

			if (cellFsOpen(f2, CELL_FS_O_CREAT | CELL_FS_O_TRUNC | CELL_FS_O_WRONLY, &fdw, 0, 0) != CELL_FS_SUCCEEDED) {
				DPrintf("Error Opening (write):\n%s\n\n", f2);
				abort_copy = 1;
				free(f1);
				free(f2);
				free(string1);
				PfsFileFindClose(dir);
				return -1;
			}
			if ((fdr = PfsFileOpen(f1)) == PFS_FILE_INVALID) {
				DPrintf("Error Opening (read):\n%s\n\n", f1);
				abort_copy = 1;
				free(f1);
				free(f2);
				free(string1);
				cellFsClose(fdw);
				PfsFileFindClose(dir);
				return -1;
			}

			PfsFileGetSizeFromHandle(fdr, &write_size);
			file_counter++;
			DPrintf("Copying %s\n\n", f1);

			while (write_end < write_size) {
				if (PfsFileRead(fdr, buf2, BUF_SIZE2, &size) != 0) {
					DPrintf("Error Read:\n%s\n\n", f1);
					abort_copy = 1;
				} else if (cellFsWrite(fdw, buf2, size, NULL) != CELL_FS_SUCCEEDED) {
					DPrintf("Error Write:\n%s\n\n", f1);
					abort_copy = 1;
				}
				if (abort_copy) {
					free(f1);
					free(f2);
					cellFsClose(fdw);
					PfsFileClose(fdr);
					PfsFileFindClose(dir);
					free(string1);
					return -1;
				}

				pad_read();
				if (new_pad & BUTTON_TRIANGLE) {
					abort_copy = 1;
					DPrintf("Aborted by user \n");
					break;
				}

				global_device_bytes += size;
				write_end += size;

				int seconds = (int) (time(NULL) - time_start);


	int eta=(copy_global_bytes-global_device_bytes)/(global_device_bytes/seconds);
	lastINC3=0;

	if(no_real_progress==1)
	{
		eta=(copy_file_counter-file_counter)/(file_counter/seconds);
		if( ( ((int)(file_counter*100ULL/copy_file_counter)) - lastINC2)>0)
		{
			lastINC2=(int) (file_counter*100ULL / copy_file_counter);
			if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}
//			if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);
		}
	}
	else
	{

		if( ( ((int)(global_device_bytes*100ULL/copy_global_bytes)) - lastINC2)>0)
		{
			lastINC2=(int) (global_device_bytes*100ULL / copy_global_bytes);
			if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}

		}
	}
		if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);

if(lastINC3>0 || (time(NULL)-seconds2)!=0 ) 
{	
	if(no_real_progress==1)
		//sprintf(string1,"Copied %1.2f MB (file %i). Elapsed: %i %2.2i min",((double) global_device_bytes)/(1024.0*1024.0), file_counter, (seconds/60), seconds % 60);
		sprintf(string1,"Copied %.0f MB (%i of %i files). Elapsed: %i:%2.2i min",((double) global_device_bytes)/(1024.0*1024.0), file_counter, copy_file_counter, (seconds/60), seconds % 60);
	else
		sprintf(string1,"Copied %1.2f of %1.2f MB. Remaining: %i:%2.2i min",((double) global_device_bytes)/(1024.0*1024.0),((double) copy_global_bytes)/(1024.0*1024.0), (eta/60), eta % 60);

	cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, string1);	

	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x101010ff);
	cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
	cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Press /\\ to abort");
	cellDbgFontDrawGcm();
	seconds2= (int) (time(NULL));
	flip(); 
}






/*				sprintf(string1, "Copying. File: %i Time: %2.2i:%2.2i:%2.2i %2.2i/100 Vol: %1.2f GB\n", file_counter,
						seconds / 3600, (seconds / 60) % 60, seconds % 60, (int) (write_end * 100ULL / write_size),
						((double) global_device_bytes) / (1024.0 * 1024. * 1024.0));

				cellGcmSetClearSurface(gCellGcmCurrentContext, CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B |	CELL_GCM_CLEAR_A);
				draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x200020ff);
				cellDbgFontPrintf(0.07f, 0.07f, 1.2f, 0xffffffff, string1);
				cellDbgFontPrintf(0.5f - 0.15f, 1.0f - 0.07 * 2.0f, 1.2f, 0xffffffff, "Hold /\\ to Abort");
				cellDbgFontDrawGcm();
				flip(); */
			}

			cellFsClose(fdw);
			cellFsChmod(f2, CELL_FS_S_IFMT | 0777);
			PfsFileClose(fdr);

			free(f1);
			free(f2);
			free(string1);
		}

		if (abort_copy)
			break;

		pad_read();
		if (new_pad & BUTTON_TRIANGLE) {
			abort_copy = 1;
			DPrintf("Aborted by user \n");
			break;
		}

	} while (PfsFileFindNext(dir, &entry) == 0);

	PfsFileFindClose(dir);
	return 0;
}


int my_game_copy_pfsm(char *path, char *path2)
{
	global_device_bytes=0x00ULL;
	lastINC=0, lastINC3=0, lastINC2=0; 
	BUF_SIZE2=(MAX_FAST_FILES)*MAX_FAST_FILE_SIZE;
	buf2 = (u8*)malloc(BUF_SIZE2);
	_my_game_copy_pfsm(path, path2);
	free(buf2);
	return 0;
}
#endif

static int _my_game_copy(char *path, char *path2)
{
	DIR  *dir;

	dir=opendir (path);
	if(!dir) {abort_copy=7;return -1;}

	while(1)
		{
		//if(to_reboot) {abort_copy=1; break;}
		struct dirent *entry=readdir (dir);
		if(!entry) break;

		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;

		if((entry->d_type & DT_DIR))
			{

			if(abort_copy) break;

			char *d1f= (char *) malloc(512);
			char *d2f= (char *) malloc(512);
			if(!d1f || !d2f) {if(d1f) free(d1f); if(d2f) free(d2f);closedir (dir);DPrintf("malloc() Error!!!\n\n");abort_copy=2;return -1;}
			sprintf(d1f,"%s/%s", path, entry->d_name);
			sprintf(d2f,"%s/%s", path2, entry->d_name);
			mkdir(path2, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
			mkdir(d2f, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

			_my_game_copy(d1f, d2f);
			free(d1f);free(d2f);
			if(abort_copy) break;
			}
		else
			{

//			char *d1= (char *) malloc(512);
//			char *d2= (char *) malloc(512);
			sprintf(d1,"%s/%s", path, entry->d_name);
			sprintf(d2,"%s/%s", path2, entry->d_name);
			if(use_symlinks==1)
			{
				if(strstr(d1, "/dev_hdd0/game/")!=NULL && strstr(d2, "/dev_hdd0/G/")!=NULL)
				{
					if(strstr(entry->d_name, "MM_NPDRM_")!=NULL)
					{
						sprintf(d1, "%s", entry->d_name);
						sprintf(d2, "%s/%s", path2, d1+9);
						sprintf(d1, "%s/%s", path, entry->d_name);
						remove(d2);
						rename(d1, d2);
					}
					file_counter++;
				}
				else
				{
					if(strstr(entry->d_name, ".PNG")==NULL 
					&& strstr(entry->d_name, "ICON1")==NULL && strstr(entry->d_name, "PIC1")==NULL 
					&& strstr(entry->d_name, ".PAM")==NULL && strstr(entry->d_name, "SND0.AT3")==NULL 
					&& strstr(entry->d_name, "ICON0")==NULL && strstr(entry->d_name, "PIC0")==NULL)
					{
	//					if(strstr(entry->d_name, "EBOOT.BIN")!=NULL)  sprintf(d2,"%s/MM_EBOOT.BIN", path2);
	//					else if(strstr(entry->d_name, ".self")!=NULL) sprintf(d2,"%s/MM_%s", path2, entry->d_name);

						if(strstr(entry->d_name, "EBOOT.BIN")==NULL && strstr(entry->d_name, ".SELF")==NULL && strstr(entry->d_name, ".self")==NULL && strstr(entry->d_name, ".SPRX")==NULL && strstr(entry->d_name, ".sprx")==NULL && strstr(entry->d_name, "PARAM.SFO")==NULL)
						{
							unlink(d2);
							remove(d2);
							link(d1, d2);
						}
						file_counter++;
					}
					else
						{
							if(fast_copy_add(path, path2, entry->d_name)<0) {abort_copy=666; closedir(dir);return -1;}
						}
				}
			}
			else
			{
				if(join_copy==0 || (join_copy==1 && strstr(entry->d_name, ".666")!=NULL))
				{
					if(strstr(entry->d_name, ".66600")!=NULL && max_joined<10) 
					{
						sprintf(file_to_join[max_joined].split_file, "%s/%s", path2, entry->d_name);
						file_to_join[max_joined].split_file[strlen(file_to_join[max_joined].split_file)-6]=0;
						max_joined++;
					}
					if(fast_copy_add(path, path2, entry->d_name)<0) {abort_copy=666; closedir(dir);return -1;}//free(d1);free(d2);
				}
			}
//				free(d1);free(d2);
			}
		
		if(abort_copy) break;
		}

	closedir(dir);
	if(abort_copy) return -1;

	return 0;

}

// test if files >= 4GB
int my_game_test(char *path, int to_abort)
{
#if (CELL_SDK_VERSION>0x210001)
	if(strstr (path,"/pvd_usb")!=NULL && pfs_enabled) 
	{
			my_game_test_pfsm(path, to_abort); 
			return 0;
	}
#endif

   DIR  *dir;
	if(abort_copy==1) return -1;
    dir=opendir (path); 
    if(!dir) return -1;


   while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;

		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;

		if((entry->d_type & DT_DIR))
			{
			
				char *d1f= (char *) malloc(512);
				num_directories++;

				if(!d1f) {closedir (dir);abort_copy=2;return -1;}
				sprintf(d1f,"%s/%s", path, entry->d_name);

				my_game_test((char*)d1f, to_abort);
				free(d1f);
				if(abort_copy) break;
			}
		else
			{
//				char *f= (char *) malloc(512);
//				struct stat s;
//				off64_t size=0LL;

//				if(!f) {abort_copy=2;closedir (dir);return -1;}
				sprintf(df,"%s/%s", path, entry->d_name);

				if(strlen(entry->d_name)>6 && to_abort!=3)
				{
					char *p= df;
					p+= strlen(df)-6; // adjust for .666xx
					if(p[0]== '.' && p[1]== '6' && p[2]== '6' && p[3]== '6')
					{
							num_files_split++;
							if(p[4]=='0' && p[5]=='0') num_files_big++;
							if(to_abort==2 || join_copy==1) 
							{
								if(stat(df, &s3)>=0)
								{
									if(s3.st_size>=0x100000000LL) num_files_big++;
									global_device_bytes+=s3.st_size;
								}
								if(strstr(df, ".66600")!=NULL && join_copy==1 && max_joined<10)
								{
									sprintf(file_to_join[max_joined].split_file, "%s", df);
									file_to_join[max_joined].split_file[strlen(file_to_join[max_joined].split_file)-6]=0;
									max_joined++;

									//abort_copy=1; break;
								}

								if(to_abort==2 && join_copy==0) {abort_copy=1; break;} 
							}

					}
				}

				if(to_abort!=2 && to_abort!=3 && join_copy==0){
					if(stat(df, &s3)<0) {abort_copy=3;break;}//if(f) free(f);break;}
					if(s3.st_size>=0x100000000LL) num_files_big++;
					global_device_bytes+=s3.st_size;
				}
			
			 	file_counter++;

				int seconds= (int) (time(NULL)-time_start);
				if((seconds>10) && to_abort==1) {abort_copy=1; break;}//if(f) free(f); //file_counter>4000 || 
				
				pad_read();
				if (new_pad & BUTTON_TRIANGLE) abort_copy=1;
			
			if(abort_copy) break;

			}

		}

	closedir (dir);
return 0;
}

int my_game_copy(char *path, char *path2)
{
	int ret3, flipF;
	disable_sc36();
	char string1[1024];
	if(progress_bar==1) {
		dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Verifying source data, please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
		flipc(60);


	}
	else
	{
		for(flipF = 0; flipF<60; flipF++) {
		sprintf(string1, "Preparing, please wait!");cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);	cellDbgFontPrintf( 0.3f, 0.45f, 0.8f, 0xc0c0c0c0, string1);	cellDbgFontDrawGcm(); flip();
		}
	}

		file_counter=0;	global_device_bytes=0ULL;abort_copy=0;

#if (CELL_SDK_VERSION>0x210001)
		if(strstr (path,"/pvd_usb")!=NULL && pfs_enabled)
		{
				my_game_test_pfsm(path, 1); 
				if(abort_copy==1)
				{
					abort_copy=0;
					file_counter=0;
					my_game_test_pfsm(path, 2); 
					abort_copy=1;
				}
		}
		else
#endif
		{
		abort_copy=0;
		if(strstr(path,"/dev_hdd0")!=NULL)
			my_game_test(path, 0); 
		else
		{
			max_joined=0;
			time_start= time(NULL);
			my_game_test(path, 1); 
			if(abort_copy==1)
			{
				abort_copy=0;
				file_counter=0;
				max_joined=0;
				time_start= time(NULL);
				my_game_test(path, 3); 
				abort_copy=1;
			}
		}
		}

		if(progress_bar==1) cellMsgDialogAbort();
		char just_drive[16]; just_drive[0]=0;
		char *pathpos=strchr(path2+1,'/');

		if(pathpos!=NULL)
		{
			strncpy(just_drive, path2, 15);
			just_drive[pathpos-path2]=0;
		}
		else
			sprintf(just_drive, "%s", path2);

		cellFsGetFreeSize(just_drive, &blockSize, &freeSize);
		freeSpace = ( (uint64_t) (blockSize * freeSize) );
		if((uint64_t)global_device_bytes>(uint64_t)freeSpace && use_symlinks!=1 && freeSpace!=0)
		{
			sprintf(string1, "Not enough space on destination drive! (Available: %.2fMB)\n\nAdditional %.2fMB of free space required!", (double) ((freeSpace)/1048576.00f), (double) ((global_device_bytes-freeSpace)/1048576.00f) );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
			abort_copy=1;
			goto return_error;
		}

		copy_file_counter=file_counter; if(copy_file_counter==0) copy_file_counter=1;
		file_counter=0;
		copy_global_bytes=global_device_bytes;
		lastINC=0; lastINC3=0; lastINC2=0;

	if(join_copy==1)
		sprintf(string1, "%s", "Installing Game Files to HDD cache, please wait...");	
	else
	{
		if(abort_copy==1)
			sprintf(string1, "Copying %d files, please wait...", copy_file_counter);
		else 
		{
			if(use_symlinks==1)
				sprintf(string1, "Creating links for %d files (%1.3f GB), please wait...", copy_file_counter, (double)(copy_global_bytes/(1024.0*1024.0*1024.0)));
			else
				sprintf(string1, "Copying %d files (%1.3f GB), please wait...", copy_file_counter, (double)(copy_global_bytes/(1024.0*1024.0*1024.0)));
		}
	}

	if(progress_bar==1) // && abort_copy==0
	{
		ret3=cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL	|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE	|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF	|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE	|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE,	string1,	NULL,	NULL,	NULL); 
		flipc(60);
	}

	no_real_progress=0;
	if(abort_copy==1) no_real_progress=1;

	abort_copy=0;
	global_device_bytes=0;
	time_start= time(NULL);

#if (CELL_SDK_VERSION>0x210001)
	if(strstr (path,"/pvd_usb")!=NULL && pfs_enabled){
		my_game_copy_pfsm(path, path2);
	}
	else
#endif
	{
		if(fast_copy_async(path, path2, 1)<0) {abort_copy=665;goto return_error;}//ret3=cellMsgDialogAbort();
		int ret=_my_game_copy(path, path2);
		int ret2= fast_copy_process();
		fast_copy_async(path, path2, 0);
		if(ret<0 || ret2<0) goto return_error;
	}
	join_copy=0;
	if(progress_bar==1) cellMsgDialogClose(60.0f);//cellMsgDialogAbort();
	enable_sc36(); flip();
	return 0;

return_error:
	join_copy=0;
	if(progress_bar==1) cellMsgDialogClose(60.0f); //cellMsgDialogAbort();
	enable_sc36(); flip();
	return -1;

}

int my_game_delete(char *path)
{
	DIR  *dir;
//	char *f= NULL;
	char string1[1024];

   dir=opendir (path);
   if(!dir) return -1;
   
   while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;
		
		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;

		if((entry->d_type & DT_DIR))
			{
			
			char *f= (char *) malloc(512);

			if(!f) {closedir (dir);DPrintf("malloc() Error!!!\n\n");abort_copy=2;return -1;}
			sprintf(f,"%s/%s", path, entry->d_name);
			my_game_delete(f);
//            		DPrintf("Deleting <%s>\n\n", path);
			if(rmdir(f)) {abort_copy=3;DPrintf("Delete error!\n -> <%s>\n\n", entry->d_name);}//break; if(d1) free(d1);
			free(f);
			if(abort_copy) break;
			file_counter--;

			goto display_mess;
			}
		else
			{

//			f=(char *) malloc(512);
//			if(!f) {DPrintf("malloc() Error!!!\n\n");abort_copy=2;closedir (dir);return -1;}
			sprintf(df,"%s/%s", path, entry->d_name);
			remove(df);//if(remove(f)) {abort_copy=3;DPrintf("Delete error!\n -> %s\n\n", f);break;} //if(f) free(f);
//			free(f);
//			DPrintf("Deleted: %s\n\n", f);
//			if(f) free(f);

			display_mess:

			int seconds= (int) (time(NULL)-time_start);
			file_counter++;
		
		if(file_counter % 32==0) {
			sprintf(string1,"Deleting files: %i  [Elapsed: %2.2i:%2.2i:%2.2i]\n", file_counter, seconds/3600, (seconds/60) % 60, seconds % 60);
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
//			draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x200020ff);
			cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
			cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Hold /\\ to Abort");
			cellDbgFontDrawGcm();
			flip();
		}
			pad_read();

			if (new_pad & BUTTON_TRIANGLE) 	abort_copy=1;
			if(abort_copy) break;

			}
		}

	closedir (dir);

	return 0;
}


static int _copy_nr(char *path, char *path2, char *path_name)
{
	DIR  *dir;

	dir=opendir (path);
	if(!dir) return -1;

	while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;

		if(entry->d_name[0]=='.' && entry->d_name[1]==0) continue;
		if(entry->d_name[0]=='.' && entry->d_name[1]=='.' && entry->d_name[2]==0) continue;

		if((entry->d_type & DT_DIR))
			{

			if(abort_copy) break;

			char *f= (char *) malloc(512);
			if(!d1) {closedir (dir); abort_copy=2; return -1;}
			sprintf(f,"%s/%s", path, entry->d_name);

			_copy_nr(f, path2, path_name);

			free(f);
			if(abort_copy) break;
			}
		else
			{


		int seconds2= (int) (time(NULL));
		char rdr[255], pathTO[512];

		int fs;
		int fd;
		uint64_t fsiz = 0;
		uint64_t msiz = 0;

		sprintf(rdr, "%s/%s", path, entry->d_name);
		sprintf(pathTO, "%s/%s", path2, entry->d_name);

		cellFsOpen(rdr, CELL_FS_O_RDONLY, &fs, NULL, 0);
		cellFsLseek(fs, 0, CELL_FS_SEEK_END, &msiz);
		cellFsClose(fs);

		uint64_t chunk = 16*1024;
		if(msiz<chunk && msiz>0) chunk=msiz;
		if(msiz<1) continue;

		char w[chunk];
		uint64_t written=0;
		cellFsOpen(rdr, CELL_FS_O_RDONLY, &fs, NULL, 0);

		remove(pathTO); abort_copy=0;
		lastINC3=0; lastINC=lastINC2;
		cellFsOpen(pathTO, CELL_FS_O_CREAT|CELL_FS_O_RDWR|CELL_FS_O_APPEND, &fd, NULL, 0);
		char *string1= (char *) malloc(512);
		while(fsiz < msiz && abort_copy==0)
		{

			if((fsiz+chunk) > msiz)
			{
				chunk = (msiz-fsiz);
				char x[chunk];
				cellFsLseek(fs,fsiz,CELL_FS_SEEK_SET, NULL);
				if(cellFsRead(fs, (void *)x, chunk, NULL)!=CELL_FS_SUCCEEDED)
					{abort_copy=1;break;}
				else
				{
					if(cellFsWrite(fd, (const void *)x, chunk, &written)!=CELL_FS_SUCCEEDED){abort_copy=1;break;};
					if(written!=chunk){abort_copy=1;break;}
					global_device_bytes+=chunk;
					break;
				}
			}
			else
			{
				cellFsLseek(fs,fsiz,CELL_FS_SEEK_SET, NULL);
				if(cellFsRead(fs, (void *)w, chunk, NULL)!=CELL_FS_SUCCEEDED){abort_copy=1;break;};
				if(cellFsWrite(fd, (const void *)w, chunk, &written)!=CELL_FS_SUCCEEDED){abort_copy=1;break;};
				if(written!=chunk){abort_copy=1;break;}
				fsiz = fsiz + chunk;
				global_device_bytes+=chunk;
			}

			int seconds= (int) (time(NULL)-time_start);
			int eta=(copy_global_bytes-global_device_bytes)/(global_device_bytes/seconds);

	lastINC3=0;
if( ( ((int)(global_device_bytes*100ULL/copy_global_bytes)) - lastINC2)>0)
{
	lastINC2=(int) (global_device_bytes*100ULL / copy_global_bytes);
	if(lastINC<lastINC2) {lastINC3=lastINC2-lastINC; lastINC=lastINC2;}

	if(lastINC3>0) cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE,lastINC3);

}


if(lastINC3>0 || (time(NULL)-seconds2)!=0 ) 
{	
	if(no_real_progress==1)
		sprintf(string1,"Copied %1.2fMB. Elapsed time: %i:%2.2i min",((double) global_device_bytes)/(1024.0*1024.0), (seconds/60), seconds % 60);
	else
		sprintf(string1,"Copied %1.2f of %1.2f MB. Remaining: %i:%2.2i min",((double) global_device_bytes)/(1024.0*1024.0),((double) copy_global_bytes)/(1024.0*1024.0), (eta/60), eta % 60);

	cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, string1);	

	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x101010ff);
	cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
	cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Press /\\ to abort");
	cellDbgFontDrawGcm();
	flip(); 
	seconds2= (int) (time(NULL));
}

	//cellSysutilCheckCallback();
	pad_read();
	if ( old_pad & BUTTON_TRIANGLE || new_pad & BUTTON_CIRCLE) {abort_copy=1; new_pad=0; old_pad=0; break;}


		}
	free(string1);


		cellFsClose(fd);
		cellFsClose(fs);
		cellFsChmod(pathTO, 0666);

		if(abort_copy) break;

		}

	}

	closedir(dir);
	if(abort_copy) return -1;

	return 0;

}


int copy_nr(char *path, char *path_new, char *path_name) // recursive to single folder copy
{
	int ret3;
	char path2[512];
	char string1[1024];
	if(strstr(path_new,"/ps3_home/video")!=NULL || strstr(path_new,"/ps3_home/music")!=NULL || strstr(path_new,"/ps3_home/photo")!=NULL)
	{
		sprintf(path2, "%s", app_temp);
		del_temp(app_temp);
	}
	else
		sprintf(path2, "%s", path_new);

	sprintf(string1, "Preparing, please wait!");
	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	cellDbgFontPrintf( 0.3f, 0.45f, 0.8f, 0xc0c0c0c0, string1);
	cellDbgFontDrawGcm();	flip();
		file_counter=0;	global_device_bytes=0;abort_copy=0;
		if(strstr(path,"/dev_hdd0")!=NULL)
				my_game_test(path, 0); 
			else
				my_game_test(path, 1); 
		copy_file_counter=file_counter; 
		copy_global_bytes=global_device_bytes;
		lastINC=0; lastINC3=0; lastINC2=0;

if(abort_copy==1)
	sprintf(string1, "Copying over %d+ files (%1.3f+ GB), please wait...", copy_file_counter, (double)(copy_global_bytes/(1024.0*1024.0*1024.0)));
	else 
	sprintf(string1, "Copying %d files (%1.3f GB), please wait...", copy_file_counter, (double)(copy_global_bytes/(1024.0*1024.0*1024.0)));

ret3=cellMsgDialogOpen2(
	CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL
	|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE
	|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF
	|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE
	|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE,
	string1,
	NULL,
	NULL,
	NULL);
	flip();

	no_real_progress=0;
	if(abort_copy==1) no_real_progress=1;

	abort_copy=0;
	global_device_bytes=0;
	lastINC=0; lastINC3=0; lastINC2=0;
	time_start= time(NULL);
	file_counter=0;

	_copy_nr((char*)path, (char*)path2, (char*)path_name);
	ret3=cellMsgDialogAbort(); flip();

	if(strstr(path_new, "/ps3_home")!=NULL) 
				{
	DIR  *dir;
	char tr[512];
	int n=0;
	for (n=0;n<256;n++ )
	{
	dir=opendir (app_temp);
	if(!dir) return -1;
	while(1)
		{
		struct dirent *entry=readdir (dir);
		if(!entry) break;

		if(!(entry->d_type & DT_DIR))
			{

			sprintf(tr, "%s", entry->d_name);
			cellDbgFontDrawGcm(); 
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); 
			set_texture( text_FMS, 1920, 48); display_img(0, 47, 1920, 60, 1920, 48, -0.15f, 1920, 48);	display_img(0, 952, 1920, 76, 1920, 48, -0.15f, 1920, 48);time ( &rawtime );	timeinfo = localtime ( &rawtime );	cellDbgFontPrintf( 0.83f, 0.895f, 0.7f ,0xc0a0a0a0, "%02d/%02d/%04d\n %s:%02d:%02d ", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, tmhour(timeinfo->tm_hour), timeinfo->tm_min, timeinfo->tm_sec);
			set_texture( text_bmpIC, 320, 320);  display_img(800, 200, 320, 176, 320, 176, 0.0f, 320, 320);
			if((strstr(tr, ".avi")!=NULL || strstr(tr, ".AVI")!=NULL || strstr(tr, ".m2ts")!=NULL || strstr(tr, ".M2TS")!=NULL || strstr(tr, ".mts")!=NULL || strstr(tr, ".MTS")!=NULL || strstr(tr, ".m2t")!=NULL || strstr(tr, ".M2T")!=NULL || strstr(tr, ".divx")!=NULL || strstr(tr, ".DIVX")!=NULL || strstr(tr, ".mpg")!=NULL || strstr(tr, ".MPG")!=NULL || strstr(tr, ".mpeg")!=NULL || strstr(tr, ".MPEG")!=NULL || strstr(tr, ".mp4")!=NULL || strstr(tr, ".MP4")!=NULL || strstr(tr, ".vob")!=NULL || strstr(tr, ".VOB")!=NULL || strstr(tr, ".wmv")!=NULL || strstr(tr, ".WMV")!=NULL || strstr(tr, ".ts")!=NULL || strstr(tr, ".TS")!=NULL || strstr(tr, ".mov")!=NULL || strstr(tr, ".MOV")!=NULL) )
			{
				cellDbgFontPrintf( 0.35f, 0.45f, 0.8f, 0xc0c0c0c0, "Adding files to video library...\n\nPlease wait!\n\n[ %s ]",tr); cellDbgFontDrawGcm();
				flip();
		 		video_export(tr, path_name, 0);
			}

			if(strstr(tr, ".mp3")!=NULL || strstr(tr, ".MP3")!=NULL || strstr(tr, ".wav")!=NULL || strstr(tr, ".WAV")!=NULL || strstr(tr, ".aac")!=NULL || strstr(tr, ".AAC")!=NULL)
			{
				cellDbgFontPrintf( 0.35f, 0.45f, 0.8f, 0xc0c0c0c0, "Adding files to music library...\n\nPlease wait!\n\n[ %s ]",tr); cellDbgFontDrawGcm();
				flip();
		 		music_export(tr, path_name, 0);
			}

			if(strstr(tr, ".jpg")!=NULL || strstr(tr, ".JPG")!=NULL || strstr(tr, ".jpeg")!=NULL || strstr(tr, ".JPEG")!=NULL || strstr(tr, ".png")!=NULL || strstr(tr, ".PNG")!=NULL)
			{
				cellDbgFontPrintf( 0.35f, 0.45f, 0.8f, 0xc0c0c0c0, "Adding files to photo library...\n\nPlease wait!\n\n[ %s ]",tr); cellDbgFontDrawGcm();
				flip();
		 		photo_export(tr, path_name, 0);
			}

			sprintf(tr, "%s/%s", app_temp, entry->d_name);
			if(stat(tr, &s3)>=0) remove(tr);
			}
		}
	closedir(dir);
	}
	video_finalize();
	music_finalize();
	photo_finalize();

	del_temp(app_temp);

				}

	return 0;
}

 
void open_osk(int for_what, char *init_text)
{
	char orig[512];

	if(for_what==1) sprintf(orig, "Rename [%s] to:", init_text);
	if(for_what==2)	sprintf(orig, "CREATE NEW FOLDER - Enter name for the new folder:");
	if(for_what==3)	sprintf(orig, "%s", init_text);
	if(for_what==4)	sprintf(orig, "%s", init_text);

    wchar_t my_message[((strlen(orig) + 1)*2)];
    mbstowcs(my_message, orig, (strlen(orig) + 1));

    wchar_t INIT_TEXT[((strlen(init_text) + 1)*2)];
    mbstowcs(INIT_TEXT, init_text, (strlen(init_text) + 1));
	if(for_what==2 || for_what==3) INIT_TEXT[0]=0;

	inputFieldInfo.message = (uint16_t*)my_message;
	inputFieldInfo.init_text = (uint16_t*)INIT_TEXT;
	inputFieldInfo.limit_length = 128;

	CellOskDialogPoint pos;
	pos.x = 0.0; pos.y = 0.5;
	int32_t LayoutMode = CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER;

	CellOskDialogParam dialogParam;

	if(for_what==3)
	{
	inputFieldInfo.limit_length = 4;
	cellOskDialogSetKeyLayoutOption (CELL_OSKDIALOG_10KEY_PANEL);
	cellOskDialogAddSupportLanguage (CELL_OSKDIALOG_PANELMODE_PASSWORD | CELL_OSKDIALOG_PANELMODE_NUMERAL);

	dialogParam.allowOskPanelFlg = ( CELL_OSKDIALOG_PANELMODE_NUMERAL | CELL_OSKDIALOG_PANELMODE_PASSWORD);
	dialogParam.firstViewPanel = CELL_OSKDIALOG_PANELMODE_NUMERAL;

	}
	else
	{
	cellOskDialogSetKeyLayoutOption (CELL_OSKDIALOG_10KEY_PANEL | CELL_OSKDIALOG_FULLKEY_PANEL);
	cellOskDialogAddSupportLanguage (CELL_OSKDIALOG_PANELMODE_ALPHABET | CELL_OSKDIALOG_PANELMODE_NUMERAL | 
		CELL_OSKDIALOG_PANELMODE_ENGLISH |
		CELL_OSKDIALOG_PANELMODE_DEFAULT |
		CELL_OSKDIALOG_PANELMODE_SPANISH |
		CELL_OSKDIALOG_PANELMODE_FRENCH |
		CELL_OSKDIALOG_PANELMODE_RUSSIAN |
		CELL_OSKDIALOG_PANELMODE_JAPANESE |
		CELL_OSKDIALOG_PANELMODE_CHINA_TRADITIONAL);

	dialogParam.allowOskPanelFlg = (CELL_OSKDIALOG_PANELMODE_ALPHABET | CELL_OSKDIALOG_PANELMODE_NUMERAL | 
		CELL_OSKDIALOG_PANELMODE_ENGLISH |
		CELL_OSKDIALOG_PANELMODE_DEFAULT |
		CELL_OSKDIALOG_PANELMODE_SPANISH |
		CELL_OSKDIALOG_PANELMODE_FRENCH |
		CELL_OSKDIALOG_PANELMODE_RUSSIAN |
		CELL_OSKDIALOG_PANELMODE_JAPANESE |
		CELL_OSKDIALOG_PANELMODE_CHINA_TRADITIONAL);
	dialogParam.firstViewPanel = CELL_OSKDIALOG_PANELMODE_ALPHABET_FULL_WIDTH;
	}

	cellOskDialogSetLayoutMode( LayoutMode );

	dialogParam.controlPoint = pos; 
	dialogParam.prohibitFlgs = CELL_OSKDIALOG_NO_RETURN;
	cellOskDialogSetInitialInputDevice(CELL_OSKDIALOG_INPUT_DEVICE_PAD );
	osk_dialog=0;
	cellOskDialogLoadAsync(memory_container, &dialogParam, &inputFieldInfo);
	osk_open=for_what;
};

//register photo

void cb_export_finish_p( int result, void *userdata) //export callback
{
//	int callback_type = (int)userdata; 
	(void) userdata;
//	sprintf(filename, "CALLBACK [%i]", result );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
	pe_result = result;
	sys_timer_usleep (1000 * 1000);

}

void cb_export_finish2_p( int result, void *userdata)
{
	pe_result = result;
	(void) userdata;
}

int photo_initialize( void ) {

	int ret = 0;

	int callback_type = CALLBACK_TYPE_INITIALIZE;
	if(pe_initialized==0)
	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_PHOTO_EXPORT);
	ret = cellPhotoExportInitialize(	CELL_PHOTO_EXPORT_UTIL_VERSION_CURRENT,
								memory_container,
								cb_export_finish2_p,
								(void*)callback_type );

	return ret;
}

int photo_register( int callback_type, char* filenape_v, const char* album) 
{

//	sprintf(filename, "REGISTER INITATED [%s] [%s]", filenape_v, album );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
	int ret = 0;
	int fnl = strlen(filenape_v);
	char filenape_v2[256];
	char temp_vf[256], temp_vf2[256], temp_vf0[256];
	sprintf(temp_vf0, "%s", "PEXPORT.ext");
	temp_vf0[10]=filenape_v[fnl-1];
	temp_vf0[ 9]=filenape_v[fnl-2];
	temp_vf0[ 8]=filenape_v[fnl-3];
	temp_vf0[ 7]=filenape_v[fnl-4];
	if(temp_vf0[7]==0x2e && temp_vf0[6]==0x2e) temp_vf0[6]=0x54;
	if(temp_vf0[10]==0x20) temp_vf0[10]=0x00;

	sprintf(temp_vf, "%s/%s", app_temp, temp_vf0);
	remove(temp_vf);
	sprintf(temp_vf2, "%s/%s", app_temp, filenape_v);
	rename(temp_vf2, temp_vf);

	CellPhotoExportSetParam param;
	sprintf(filenape_v2, "%s", filenape_v); filenape_v2[128]=0;
	char *pch=filenape_v2;
	char *pathpos=strrchr(pch,'.');	
	if(pathpos!=NULL) filenape_v2[pathpos-pch]=0; //remove extension

	param.photo_title  = (char*)filenape_v2;
	param.game_title  = (char*)album;
	param.game_comment  = (char*)"Transferred by multiMAN";
	param.reserved  = NULL;

	ret = cellPhotoExportFromFile( app_temp,
								temp_vf0,//filenape_v
								&param,
								cb_export_finish2_p,
								(void*)callback_type );

	return ret;
}

int photo_finalize( void ) {

	int ret = 0;
	int callback_type = CALLBACK_TYPE_FINALIZE;

	if(pe_initialized==1)
	ret = cellPhotoExportFinalize(	cb_export_finish_p,
								(void*)callback_type );
	pe_initialized=0;
	return ret;
}

void photo_export( char *filenape_v, char *album, int to_unregister )
{
	int ret = 0;
	pe_result = 123;

	if(pe_initialized==0) {
	ret = photo_initialize();

	while (1) {
		//cellSysutilCheckCallback();
		if(pe_result < 1) break;
		if(pe_result == CELL_PHOTO_EXPORT_UTIL_RET_OK  || pe_result == CELL_PHOTO_EXPORT_UTIL_RET_CANCEL || pe_result == CELL_OK) break;
	}
		if(pe_result != CELL_PHOTO_EXPORT_UTIL_RET_CANCEL) pe_initialized=1;
	}

	if(pe_result == CELL_PHOTO_EXPORT_UTIL_RET_OK  || pe_result == CELL_OK || pe_result==0 || pe_initialized==1)
	{

	const int callback_type1 = CALLBACK_TYPE_REGIST_1;

	pe_result = 123;
	ret = photo_register(callback_type1, filenape_v, album);

//	else
		{
		while (1) {
			//cellSysutilCheckCallback();
			if(pe_result < 1) break;
			if(pe_result == CELL_PHOTO_EXPORT_UTIL_RET_OK || pe_result == CELL_PHOTO_EXPORT_UTIL_RET_CANCEL || pe_result == CELL_OK) break;
			sys_timer_usleep (500 * 1000);
//			flip();
		}
		}

		if(to_unregister==1)
		{
		pe_result = 123;
		ret = photo_finalize();

		while (1) {
			//cellSysutilCheckCallback();
			if(pe_result < 1) break;
			if(pe_result == CELL_PHOTO_EXPORT_UTIL_RET_OK || pe_result == CELL_PHOTO_EXPORT_UTIL_RET_CANCEL || pe_result == CELL_OK) break;
			sys_timer_usleep (50 * 1000);
//			flip();
		}
		}
	}

}


//register music

void cb_export_finish_m( int result, void *userdata) //export callback
{
//	int callback_type = (int)userdata; 
	(void) userdata;
//	sprintf(filename, "CALLBACK [%i]", result );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
	me_result = result;
	sys_timer_usleep (1000 * 1000);

}

void cb_export_finish2_m( int result, void *userdata)
{
	me_result = result;
	(void) userdata;
}

int music_initialize( void ) {

	int ret = 0;

	int callback_type = CALLBACK_TYPE_INITIALIZE;
	if(me_initialized==0)
	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_MUSIC_EXPORT);
	ret = cellMusicExportInitialize(	CELL_MUSIC_EXPORT_UTIL_VERSION_CURRENT,
								memory_container,
								cb_export_finish2_m,
								(void*)callback_type );

	return ret;
}

int music_register( int callback_type, char* filename_v, const char* album) 
{

//	sprintf(filename, "REGISTER INITATED [%s] [%s]", filename_v, album );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
	int ret = 0;
	int fnl = strlen(filename_v);
	char filename_v2[256];
	char temp_vf[256], temp_vf2[256], temp_vf0[256];
	sprintf(temp_vf0, "%s", "MEXPORT.ext");
	temp_vf0[10]=filename_v[fnl-1];
	temp_vf0[ 9]=filename_v[fnl-2];
	temp_vf0[ 8]=filename_v[fnl-3];
	temp_vf0[ 7]=filename_v[fnl-4];
	if(temp_vf0[7]==0x2e && temp_vf0[6]==0x2e) temp_vf0[6]=0x54;
	if(temp_vf0[10]==0x20) temp_vf0[10]=0x00;

	sprintf(temp_vf, "%s/%s", app_temp, temp_vf0);
	remove(temp_vf);
	sprintf(temp_vf2, "%s/%s", app_temp, filename_v);
	rename(temp_vf2, temp_vf);

//	sprintf(filename, "REGISTER INIT [%s] [%s]", temp_vf, temp_vf0 );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();

	CellMusicExportSetParam param;
	sprintf(filename_v2, "%s", filename_v); filename_v2[128]=0;
	char *pch=filename_v2;
	char *pathpos=strrchr(pch,'.');	
	if(pathpos!=NULL) filename_v2[pathpos-pch]=0; //remove extension

/*	for(ret = 0; ret<fnl; ret++)
	{
		if(filename_v[ret]==0x2e) filename_v2[ret]=0x20; else filename_v2[ret]=filename_v[ret];
		if(filename_v2[ret]==0x20) filename_v2[ret]=0x5f;
		filename_v2[ret+1]=0;
	}
	filename_v2[fnl]=0; */
	param.title  = (char*)filename_v2;//(char*)"Test music sample";
	param.artist  = NULL;
	param.genre  = NULL;
	param.game_title  = (char*)album;//filename_v;//NULL;
	param.game_comment  = (char*)"Transferred by multiMAN";

//#if (CELL_SDK_VERSION<=0x210001)
	param.reserved1  = NULL;
//#else
//	param.editable = 1;
//#endif
	param.reserved2  = NULL;

	ret = cellMusicExportFromFile( app_temp,
								temp_vf0,//filename_v
								&param,
								cb_export_finish2_m,
								(void*)callback_type );

//		sprintf(filename, "REGISTER INITATED [%i]", ret );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
/*	if( ret != CELL_MUSIC_EXPORT_UTIL_RET_OK && ret != CELL_OK)
	{
		ret = sys_memory_container_destroy( memory_container );
	}*/

	return ret;
}

int music_finalize( void ) {

	int ret = 0;
	int callback_type = CALLBACK_TYPE_FINALIZE;

	if(me_initialized==1)
	ret = cellMusicExportFinalize(	cb_export_finish_m,
								(void*)callback_type );
	me_initialized=0;
	return ret;
}

void music_export( char *filename_v, char *album, int to_unregister )
{
	int ret = 0;
	me_result = 123;

	if(me_initialized==0) {
	ret = music_initialize();

	while (1) {
		cellSysutilCheckCallback();
		sys_timer_usleep (500 * 1000);
		if(me_result < 1) break;
		if(me_result == CELL_MUSIC_EXPORT_UTIL_RET_OK  || me_result == CELL_MUSIC_EXPORT_UTIL_RET_CANCEL || me_result == CELL_OK) break;
	}
		if(me_result != CELL_MUSIC_EXPORT_UTIL_RET_CANCEL) me_initialized=1;
	}

	if(me_result == CELL_MUSIC_EXPORT_UTIL_RET_OK  || me_result == CELL_OK || me_result==0 || me_initialized==1)
	{

	const int callback_type1 = CALLBACK_TYPE_REGIST_1;

	me_result = 123;
	ret = music_register(callback_type1, filename_v, album);

//	else
		{
		while (1) {
			cellSysutilCheckCallback();
			if(me_result < 1) break;
			if(me_result == CELL_MUSIC_EXPORT_UTIL_RET_OK || me_result == CELL_MUSIC_EXPORT_UTIL_RET_CANCEL || me_result == CELL_OK) break;
			sys_timer_usleep (500 * 1000);
//			flip();
		}
		}

		if(to_unregister==1)
		{
		me_result = 123;
		ret = music_finalize();

		while (1) {
			cellSysutilCheckCallback();
			if(me_result < 1) break;
			if(me_result == CELL_MUSIC_EXPORT_UTIL_RET_OK || me_result == CELL_MUSIC_EXPORT_UTIL_RET_CANCEL || me_result == CELL_OK) break;
			sys_timer_usleep (50 * 1000);
//			flip();
		}
		}
	}

}





//register video

void cb_export_finish( int result, void *userdata) //export callback
{
	int callback_type = (int)userdata;

//	sprintf(filename, "CALLBACK [%i]", result );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
	ve_result = result;
	sys_timer_usleep (1000 * 1000);

	switch(result) {
	case CELL_VIDEO_EXPORT_UTIL_RET_OK: break;
	case CELL_VIDEO_EXPORT_UTIL_RET_CANCEL:
//		ret = sys_memory_container_destroy( memory_container );
		break;
	default:
		break;
	}
	
	if( callback_type == CALLBACK_TYPE_FINALIZE )
	{
//		ret = sys_memory_container_destroy( memory_container );
	}
}

void cb_export_finish2( int result, void *userdata)
{
	ve_result = result;
	(void) userdata;
}

int video_initialize( void ) {

	int ret = 0;
	


	int callback_type = CALLBACK_TYPE_INITIALIZE;
	if(ve_initialized==0)
	ret = cellSysmoduleLoadModule(CELL_SYSMODULE_VIDEO_EXPORT);
	ret = cellVideoExportInitialize(	CELL_VIDEO_EXPORT_UTIL_VERSION_CURRENT,
								memory_container,
								cb_export_finish2,
								(void*)callback_type );

//	if( ret != CELL_VIDEO_EXPORT_UTIL_RET_OK ) {
//		sys_memory_container_destroy( memory_container );
//	}
	return ret;
}

int video_register( int callback_type, char* filename_v, const char* album) 
{

//	sprintf(filename, "REGISTER INITATED [%s] [%s]", filename_v, album );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
	int ret = 0;
	int fnl = strlen(filename_v);
	char filename_v2[256];
	char temp_vf[256], temp_vf2[256], temp_vf0[256];
	sprintf(temp_vf0, "%s", "VEXPOR..ext");
	temp_vf0[10]=filename_v[fnl-1];
	temp_vf0[ 9]=filename_v[fnl-2];
	temp_vf0[ 8]=filename_v[fnl-3];
	temp_vf0[ 7]=filename_v[fnl-4];
	if(temp_vf0[7]==0x2e && temp_vf0[6]==0x2e) temp_vf0[6]=0x54;
	if(temp_vf0[10]==0x20) temp_vf0[10]=0x00;

	sprintf(temp_vf, "%s/%s", app_temp, temp_vf0);
	remove(temp_vf);
	sprintf(temp_vf2, "%s/%s", app_temp, filename_v);
	rename(temp_vf2, temp_vf);

//	sprintf(filename, "REGISTER INIT [%s] [%s]", temp_vf, temp_vf0 );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();

	CellVideoExportSetParam param;
	sprintf(filename_v2, "%s", filename_v); filename_v2[128]=0;
	char *pch=filename_v2;
	char *pathpos=strrchr(pch,'.');	
	if(pathpos!=NULL) filename_v2[pathpos-pch]=0; //remove extension

/*	for(ret = 0; ret<fnl; ret++)
	{
		if(filename_v[ret]==0x2e) filename_v2[ret]=0x20; else filename_v2[ret]=filename_v[ret];
		if(filename_v2[ret]==0x20) filename_v2[ret]=0x5f;
		filename_v2[ret+1]=0;
	}
	filename_v2[fnl]=0; */
	param.title  = (char*)filename_v2;//(char*)"Test video sample";
	param.game_title  = (char*)album;//filename_v;//NULL;
	param.game_comment  = (char*)"Transferred by multiMAN";

#if (CELL_SDK_VERSION<=0x210001)
	param.reserved1  = NULL;
#else
	param.editable = 1;
#endif
	param.reserved2  = NULL;

/*	param.title  = (char*)filename_v;//(char*)"Test video sample";
	param.game_title  = (char*)album;//filename_v;//NULL;
	param.game_comment  = (char*)"Transferred by multiMAN";
	param.editable = 0;
	param.reserved2  = NULL; */

	ret = cellVideoExportFromFile( app_temp,
								temp_vf0,//filename_v
								&param,
								cb_export_finish2,
								(void*)callback_type );

//		sprintf(filename, "REGISTER INITATED [%i]", ret );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
/*	if( ret != CELL_VIDEO_EXPORT_UTIL_RET_OK && ret != CELL_OK)
	{
		ret = sys_memory_container_destroy( memory_container );
	}*/

	return ret;
}

int video_finalize( void ) {

	int ret = 0;
	int callback_type = CALLBACK_TYPE_FINALIZE;

	if(ve_initialized==1)
	ret = cellVideoExportFinalize(	cb_export_finish,
								(void*)callback_type );
	ve_initialized=0;
//	if( ret != CELL_VIDEO_EXPORT_UTIL_RET_OK )
//	{
//		ret = sys_memory_container_destroy( memory_container );
//	}
	return ret;
}

void video_export( char *filename_v, char *album, int to_unregister )
{
	int ret = 0;
	ve_result = 123;

	if(ve_initialized==0) {
	ret = video_initialize();

	while (1) {
		cellSysutilCheckCallback();
		if(ve_result < 1) break;
		if(ve_result == CELL_VIDEO_EXPORT_UTIL_RET_OK  || ve_result == CELL_VIDEO_EXPORT_UTIL_RET_CANCEL || ve_result == CELL_OK) break;
		sys_timer_usleep (500 * 1000);
	}
		if(ve_result != CELL_VIDEO_EXPORT_UTIL_RET_CANCEL) ve_initialized=1;
	}

	if(ve_result == CELL_VIDEO_EXPORT_UTIL_RET_OK  || ve_result == CELL_OK || ve_result==0 || ve_initialized==1)
	{

	const int callback_type1 = CALLBACK_TYPE_REGIST_1;

	ve_result = 123;
	ret = video_register(callback_type1, filename_v, album);
//	sys_timer_usleep (2 * 1000 * 1000); //2sec wait
//	if(ve_result == CELL_VIDEO_EXPORT_UTIL_RET_OK || ve_result == CELL_OK) 
//		ret = video_finalize();
//	else
		{
		while (1) {
			cellSysutilCheckCallback();
			if(ve_result < 1) break;
			if(ve_result == CELL_VIDEO_EXPORT_UTIL_RET_OK || ve_result == CELL_VIDEO_EXPORT_UTIL_RET_CANCEL || ve_result == CELL_OK) break;
			sys_timer_usleep (500 * 1000);
//			flip();
		}
		}

		if(to_unregister==1)
		{
		ve_result = 123;
		ret = video_finalize();

		while (1) {
			cellSysutilCheckCallback();
			if(ve_result < 1) break;
			if(ve_result == CELL_VIDEO_EXPORT_UTIL_RET_OK || ve_result == CELL_VIDEO_EXPORT_UTIL_RET_CANCEL || ve_result == CELL_OK) break;
			sys_timer_usleep (500 * 1000);
//			flip();
		}
		}
	}
//	ret = sys_memory_container_destroy( memory_container );
}





//MP3

extern volatile bool s_receivedExitGameRequest;
static bool s_multiStreamUpdateThreadRunning;


/**********************************************************************************/
// global variables
/**********************************************************************************/
static float*        s_pOutputBuffers        = NULL;

long	fps60 = 1000000 / 60;

long nSizeSampleData = 0;
long pSampleData = NULL;

int nChannel;
int nDSPHandle;

CellMSFXReverbParams rparam;

int SUBNUM = 1;
int PLAYSUB = SUBNUM | CELL_MS_BUS_FLAG;

/**********************************************************************************/
// Function Prototypes
/**********************************************************************************/
long TriggerStream( long nCh, long pSampleData1, long pSampleData2, const long nSize, const long nSize2, const long nFrequency, const long nSampleChannels );

/**********************************************************************************/
// Static Function Prototypes
/**********************************************************************************/



/**********************************************************************************/
// The main function
/**********************************************************************************/

int LoadMP3(const char *mp3filename,long *addr, long *size, int *_mp3_freq)
{
unsigned int tSize=0;	// total size
float tTime=0;			// total time
int ret;
CellMSMP3FrameHeader Hdr;
unsigned int offset=0;


	if(!(mp3filename && addr && size))
		return -1;


	sys_vm_invalidate(vm, MB(vm_real_size));
	sys_vm_touch(vm, MB(vm_real_size));

    // Open PCM sample data
    int nFileHandle=OpenFile( mp3filename, size);

    if(nFileHandle<0)
    {
		memset((void*)pDataB, 0x00 , _mp3_buffer);
        return -1;
    }

    // Allocate memory for a sample buffer
//	if(multiStreamStarted==0)
//	pData = (long)memalign(128,*size);

	pData=pDataB;
	// Load file.
	if(*size>_mp3_buffer) *size=_mp3_buffer;
	memset((void*)pDataB, 0x00 , (*size));
	LoadFile(nFileHandle, pData, *size, 0, 0);

	*addr=pData;
//	printf("data size: 0x%x\n",(int)*size);

	while(1)
	{
		ret=cellMSMP3GetFrameInfo((void*)pData,&Hdr);
		if (ret==-1)
		{
//			printf("Invalid MP3 header\n");
//			printf("Offset=0x%x\n",offset);
			return (-1);	// Invalid MP3 header
		}

		tSize+=Hdr.PacketSize;	// Update total file size
		if ((Hdr.ID3==0)&&(Hdr.Tag==0))
			tTime+=Hdr.PacketTime;	// Update total playing time (in seconds)
		pData+=Hdr.PacketSize;	// Move forward to next packet
		offset+=Hdr.PacketSize;

// ** Display packet information **
// Using the packet size and packet time information, it is possible to build "Seek Tables".
// Then, by knowing approximately what time (in seconds) you require to playback from,
// you can start playback from the closest data packet by searching for the closest record in the table.

/*		if (Hdr.ID3!=0)
		{
			printf("Found ID3 Info\n");
			printf("Version: %x.%x\n",Hdr.ID3>>8, Hdr.ID3&255);
		}
		else if (Hdr.Tag!=0)
		{
			printf("Found Tag info\n");
		}
		else
		{
			printf("Sync: 0x%x\n",Hdr.Sync);
			printf("ID: 0x%x\n",Hdr.ID);
			printf("Layer: 0x%x\n",Hdr.Layer);
			printf("ProtBit: 0x%x\n",Hdr.ProtBit);
			printf("BitRate: %d\n",Hdr.BitRate);
			printf("Frequency: %d\n",Hdr.Frequency);
			printf("PadBit: 0x%x\n",Hdr.PadBit);
			printf("PrivBit: 0x%x\n",Hdr.PrivBit);
			printf("Mode: 0x%x\n",Hdr.Mode);
			printf("Copy: 0x%x\n",Hdr.Copy);
			printf("Home: 0x%x\n",Hdr.Home);
			printf("Emphasis: 0x%x\n",Hdr.Emphesis);
			printf("Packet Time (secs): %f\n",Hdr.PacketTime);
		} */
//		printf("Packet Size (bytes): 0x%x\n",Hdr.PacketSize);

		if (tSize==(unsigned int)*size)
		{
//			printf("MP3 File is valid.\n");
			*_mp3_freq=Hdr.Frequency;
			mp3_durr=(int)tTime;
			//sprintf(mp3_now_playing,"%d Hz, (%imin %2.2isec) [%s]", Hdr.Frequency, ((int) tTime / 60), ((int) tTime % 60), mp3filename);
//			printf("Total Playback Time at %d Hz = (secs): %f\n",Hdr.Frequency,tTime);
			return(nFileHandle);
		}
		else if (tSize>(unsigned int)*size)
		{
//			printf("ERROR: PASSED END OF FILE!\n");
//			printf("%x,%x\n",tSize,(int)*size);
			return -1;
		}
	}
}

void main_mp3( char *temp_mp3)
{
	sprintf(force_mp3_file, "%s", temp_mp3);
	force_mp3=true;
}

int main_mp3_th( char *temp_mp3)
{

	char my_mp3[1024];
	sprintf (my_mp3, "%s", temp_mp3);
	if(strstr(my_mp3, "/pvd_usb")!=NULL)
	{
		sprintf(my_mp3, "%s/TEMP/MUSIC.TMP", app_usrdir);
		file_copy(temp_mp3, my_mp3, 0);
	}

/*
			if(strstr(my_mp3, "SOUND.BIN")==NULL && strstr(my_mp3, "SILENCE.BIN")==NULL){
				char *pathpos=strrchr(my_mp3,'/');
				if(pathpos!=NULL){
					load_texture(text_bmpUPSR, playBGR, 1920);
					generateTexture( text_bmpUPSR, 1920, 1080, pathpos+1);
				}
			}
			*/
//	int i;
	int returnVal;

	// bus volumes. (Maximum volume for each speaker)
	float fBusVols[64] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};


//    if( !LoadModules() )
//    {
//        printf( "ERROR loading modules, quitting\n" );
//        return -1;
//    }

//	sys_spu_initialize(6, 0);

//	CellAudioPortParam audioParam;
//	CellAudioPortConfig portConfig;

//	if(portNum==-1)

	if(multiStreamStarted==0) 
		InitialiseAudio(MAX_STREAMS, MAX_SUBS, portNum, audioParam, portConfig);
//	else
//		free(mp3Memory);

	// Initialise MP3

	if(multiStreamStarted==0) {
	sizeNeeded=cellMSMP3GetNeededMemorySize(2);	// Maximum 256 mono MP3's playing at one time
	mp3Memory=(int*)malloc(sizeNeeded);
	if(mp3Memory==NULL) return -1;
//	printf("MP3: Space required: 0x%x. Memory addr: 0x%x\n",sizeNeeded, (int)*mp3Memory);
	if ( (returnVal = cellMSMP3Init(2, (void*)mp3Memory)) != 0 ) return -1;

	}

	// Setup the volumes on sub buss 1. By default all sub busses route to the master bus
	cellMSCoreSetVolume64(PLAYSUB, CELL_MS_WET, fBusVols);

	// Setup the volumes on the master buss
	cellMSCoreSetVolume64(CELL_MS_MASTER_BUS, CELL_MS_DRY, fBusVols);

	if(multiStreamStarted==0) {
	    cellMSSystemConfigureLibAudio(&audioParam, &portConfig);
		StartMultiStreamUpdateThread(_Multi_Stream_Update_Thread); 
		nChannel = cellMSStreamOpen();		}

	else
		{
//		cellAudioPortStop(portNum);
//		cellMSStreamClose(nChannel);
//		cellMSCoreStop(nChannel, 0);
		}

	multiStreamStarted=1;
	s_multiStreamUpdateThreadRunning = true;

// Load MP3 file
	cellMSCoreInit(nChannel);
	mp3_freq=44100;
	LoadMP3((char*) my_mp3, &pSampleData, &nSizeSampleData, &mp3_freq);

// Open streams channel
//    nChannel = cellMSStreamOpen();



//	sys_timer_usleep(fps60*60*2);	// Wait for audio to settle..
//nChannel =
	TriggerStream(	nChannel,
								pSampleData,		// 1st buffer
								pSampleData,					// 2nd buffer
								nSizeSampleData, 	// size of data (in bytes)
								nSizeSampleData, 					// size of data (in bytes)
						        mp3_freq,//SAMPLE_FREQUENCY,
						        SAMPLE_CHANNELS);
	if(nChannel == -1){printf("Trigger Stream Error: \n"); return -1;}

// ----- Main Loop -----

/*
    while(!s_receivedExitGameRequest)
    {
//		flip();
		pad_read();
		if(new_pad & BUTTON_TRIANGLE) break;

		i=cellMSStreamGetStatus(nChannel);
		if (i==CELL_MS_STREAM_OFF)
		{
			sys_timer_usleep(fps60*60*1);	// Wait a sec..

			cellMSCoreInit(nChannel);
			nChannel = TriggerStream(	nChannel,
								pSampleData,		// 1st buffer
								0,					// 2nd buffer
								nSizeSampleData, 	// size of data (in bytes)
								0, 					// size of data (in bytes)
						        SAMPLE_FREQUENCY,
						        SAMPLE_CHANNELS);
			if(nChannel == -1){printf("Trigger Stream Error: \n"); return -1;}
		}

		sys_timer_usleep(fps60);	// Update at 60 Frames Per Second

		int ret = cellSysutilCheckCallback();
		if (ret != CELL_OK) {
			printf("error: cellSysutilCheckCallback() = 0x%x\n", ret);
			return ret;
		}
	}
*/	
	//Wait for the MultiStream update thread to end

/*
	while(s_multiStreamUpdateThreadRunning){
		sys_timer_usleep(fps60); 
		flip();
		cellSysutilCheckCallback();
		pad_read();
		if(new_pad & BUTTON_TRIANGLE) break;
	}

cellMSStreamClose(nChannel);
cellMSCoreStop(nChannel, 0);

*/

//	if(multiStreamStarted!=0) ShutdownMultiStream();
//	ShutdownMultiStream();


//    UnloadModules();
//	free(mp3Memory);
    return 0;
}


/**********************************************************************************
TriggerStream
	Starts the streaming of the passed sample data as a one shot sfx.

	Requires:		pSampleData1 		Address of PCM sample data (first buffer)
					pSampleData2		Address of PCM sample data (second buffer)
					nSize				Size (in bytes) of sample data (first buffer)
					nSize2				Size (in bytes) of sample data (second buffer)
					nFrequency			Required playback frequency (in Hz)
					nSampleChannels		Number of channels within sample data (1=mono, 2 = stereo..)

	Returns:		nChannel			Stream channel number
					-1					Failed (no free channels or invalid sample data address)
**********************************************************************************/
long TriggerStream( long nCh,long pSampleData1, long pSampleData2, const long nSize, const long nSize2, const long nFrequency, const long nSampleChannels )
{
CellMSInfo  MS_Info;
float vol;

	//suppress compiler warnings
	SUPPRESS_COMPILER_WARNING(nSampleChannels);


    MS_Info.SubBusGroup         = CELL_MS_MASTER_BUS;

// Set address and size of data to play

    MS_Info.FirstBuffer         = (void *)((long)pSampleData1);
    MS_Info.FirstBufferSize     = nSize; // size in bytes
    MS_Info.SecondBuffer         = (void *)((long)pSampleData2);
    MS_Info.SecondBufferSize     = nSize2; // size in bytes

	// Set pitch and number of channels
    MS_Info.Pitch               = nFrequency;
    MS_Info.numChannels         = 2;
	MS_Info.flags				= 0;

	// Initial delay (in samples) before playback starts. Allows for sample accurate playback
	MS_Info.initialOffset		= 0;

	// Input data type
	MS_Info.inputType = CELL_MS_MP3;

    cellMSStreamSetInfo(nCh, &MS_Info);

// Note: No callback has been setup. This sound will just constantly play. No callbacks required.
	(void) cellMSSystemSetGlobalCallbackFunc(mp3_callback); 

    cellMSStreamPlay(nCh);

	vol=mp3_volume;
	cellMSCoreSetVolume1(nCh, CELL_MS_DRY, CELL_MS_SPEAKER_FL, CELL_MS_CHANNEL_0, vol);
	cellMSCoreSetVolume1(nCh, CELL_MS_DRY, CELL_MS_SPEAKER_FR, CELL_MS_CHANNEL_1, vol);
	cellMSCoreSetVolume1(nCh, CELL_MS_DRY, CELL_MS_SPEAKER_FC,  CELL_MS_CHANNEL_0, vol);
	cellMSCoreSetVolume1(nCh, CELL_MS_DRY, CELL_MS_SPEAKER_RL,  CELL_MS_CHANNEL_0, vol-0.1);
	cellMSCoreSetVolume1(nCh, CELL_MS_DRY, CELL_MS_SPEAKER_RR,  CELL_MS_CHANNEL_1, vol-0.1);
	cellMSCoreSetVolume1(nCh, CELL_MS_DRY, CELL_MS_SPEAKER_LFE, CELL_MS_CHANNEL_1, 0.1f);

    return nCh;
}

/**********************************************************************************
MultiStreamUpdateThread

	This thread updates MultiStream and libAudio:

	1) Updates MultiStream to generate more PCM data.
	2) Any MultiStream callbacks are then called
	3) Output PCM data is sent to libaudio
	4) It waits (allowing other threads to run) for lib audio to process data before repeating process
**********************************************************************************/
static void _Multi_Stream_Update_Thread(uint64_t param)
{
	(void)param;

    cellAudioPortStart(portNum);
	while(!s_receivedExitGameRequest)
	{
		sys_timer_usleep(fps60/32);
		s_pOutputBuffers = cellMSSystemSignalSPU();
		cellMSSystemGenerateCallbacks();
	}
	cellAudioPortStop(portNum);
    s_multiStreamUpdateThreadRunning = false;
//    sys_ppu_thread_exit(0);
}

int readmem(unsigned char *_x, uint64_t _fsiz, uint64_t _chunk) //read lv2 memory chunk
{

	uint64_t n, m;
	uint64_t val;

    for(n = 0; n < _chunk; n += 8) {
		 if((_fsiz + n)>0x7ffff8ULL) return (int)(n-8);
         val = peekq(0x8000000000000000ULL + _fsiz + n);
		 for(m = 0; m<8; m++) {
			 _x[n+7-m] = (unsigned char) ((val >> (m*8)) & 0x00000000000000ffULL);
		 }
	}

	return _chunk;
}


//replacemem( 0x6170705F686F6D65UUL, 0x6465765F62647664UUL); //app_home -> dev_dvd
void replacemem(uint64_t _val_search1, uint64_t _val_replace1)
{

	uint64_t n;

    for(n = 0; n < 0x7ffff8ULL; n ++) 
	{

        if( peekq(0x8000000000000000ULL + n) == _val_search1 )
		{
			pokeq(0x8000000000000000ULL + n, _val_replace1);
			n+=8;
		}
	}
	return;
}


int mod_mount_table(const char *new_path, int _mode) //mode 0/1 = reset/change
{
	if(c_firmware!=3.41f && c_firmware!=3.55f && c_firmware!=3.15f) return 0;

	uint64_t dev_table; // mount table vector
	uint64_t c_addr;
	if(c_firmware==3.15f) dev_table=peekq(0x80000000002ED750ULL);
	if(c_firmware==3.41f) dev_table=peekq(0x80000000002EDEF0ULL);
	if(c_firmware==3.55f) dev_table=peekq(0x80000000002DFC60ULL);
	int dev_table_len = 0x1400, n=0, found=0;

	uint64_t dev_bdvd_val=0x6465765F62647664ULL; // dev_bdvd
	uint64_t tmp_bdvd_val=0x746D705F62647664ULL; // tmp_bdvd
	uint64_t bdvd_val    =0x765F626476640000ULL; //   v_bdvd

//	uint64_t host_root_val0=0x686F73745F726F6FULL; // host_roo
//	uint64_t host_root_val1=0x7400000000000000ULL; // t

//	uint64_t app_home_val=0x6170705F686F6D65ULL; // app_home

	uint64_t dev_usb0_val_0=0x6465765F75736230ULL; // dev_usb000
	uint64_t dev_usb0_val_1=0x0000000000000000ULL;

	if(_mode==0) //reset mount table
	{
		for(n=0; n < dev_table_len; n++)
		{
			c_addr = dev_table + (uint64_t) n;
			if(peekq( c_addr ) == tmp_bdvd_val) 
			{ 
				pokeq( c_addr, dev_bdvd_val ); //restore dev_bdvd
				n+=8;
				found=1;
			}

			else if(peekq( c_addr ) == dev_bdvd_val && peekq ( c_addr + 16 ) == dev_usb0_val_0) 
			{
				pokeq( c_addr	 , peekq ( c_addr + 16 ) ); //restore dev_usb
				pokeq( c_addr + 8, peekq ( c_addr + 24 ) );

				pokeq( c_addr + 16, 0x00ULL ); //Clear dev_usb backup string
				pokeq( c_addr + 24, 0x00ULL );
				n+=32;
				found=1;
			}

/*			else if(peekq( c_addr ) == dev_bdvd_val && peekq ( c_addr + 16 ) == app_home_val) 
			{
				pokeq( c_addr	  , app_home_val ); //restore app_home
				pokeq( c_addr + 16, 0x00ULL);

				n+=24;
				found=1;
			}
*/
			//else if(peekq( c_addr ) == dev_bdvd_val) found=1;

		}
		return 1;//found;
	}

	if(_mode==1) //change mount table
	{
		unsigned char v1, v2;
		v1 = new_path[9];
		v2 = new_path[10];
		dev_usb0_val_1 = 0x0000000000000000ULL | ( ((uint64_t) v1 ) << 56 ) |  ( (uint64_t) v2 << 48 );
		for(n=0; n < dev_table_len; n++)
		{
			c_addr = dev_table + (uint64_t) n;
			if( (peekq( c_addr ) == dev_usb0_val_0) && (peekq( c_addr + 8ULL ) == dev_usb0_val_1)) 
			{
				pokeq( c_addr + 2ULL, bdvd_val ); //change v_usb00x to v_bdvd
				pokeq( c_addr + 16ULL, dev_usb0_val_0 ); // and backup dev_usb
				pokeq( c_addr + 24ULL, dev_usb0_val_1 ); // for later restore
				n+=32;
				if(peekq( c_addr + 2ULL) != bdvd_val ) 
					found=0;
				else
					found=1;
			}

			else if(peekq( c_addr ) == dev_bdvd_val) 
			{
				pokeq( c_addr	     , tmp_bdvd_val ); // map dev_bdvd to tmp_bdvd
				n+=8;
				found=1;
			}

		}
		return found;

	}

/*	if(_mode==2) //change app_home to dev_bdvd only
	{
		for(n=0; n < dev_table_len; n++)
		{
			c_addr = dev_table + (uint64_t) n;
			if( (peekq( c_addr ) == app_home_val) ) 
			{
				pokeq( c_addr , dev_bdvd_val ); // change app_home to dev_bdvd
				pokeq( c_addr + 8ULL, 0x0ULL ); 
				pokeq( c_addr + 16ULL, app_home_val ); // for later restore
				n+=24;
				found=1;
				return found;
			}

		}
	}
*/
	return 1;
}


void draw_dir_pane( t_dir_pane *list, int pane_size, int first_to_show, int max_lines, float x_offset)
{
	float y_offset=0.12f+new_offset;
	int e=first_to_show;
	float e_size=0.0f;
	char str[128], e_stype[8], e_name[256], e_sizet[16], this_dev[128], other_dev[128], e_date[16], str_date[16], temp_pane[1024], entry_name[512], e_attributes[16], e_attributes2[48];
	char filename[1024];
	char string1[1024];
	u32 color=0xc0c0c0c0;


	if(x_offset>=0.54f) {
		sprintf(this_pane, "%s", current_right_pane);
		sprintf(other_pane, "%s", current_left_pane);}
		else {
		sprintf(this_pane, "%s", current_left_pane);
		sprintf(other_pane, "%s", current_right_pane);}

		char temp[256]="*";	sprintf(temp, "%s/", this_pane);	temp[0]=0x30; char *pch=strchr(temp,'/');
		if(pch!=NULL) {temp[pch-temp]=0; temp[0]=0x2f;} temp[127]=0;
		strncpy(this_dev, temp, 128);
		sprintf(temp, "%s/", other_pane);	temp[0]=0x30; pch=strchr(temp,'/');
		if(pch!=NULL) {temp[pch-temp]=0; temp[0]=0x2f;} temp[127]=0;
		strncpy(other_dev, temp, 128);

	if(first_to_show>pane_size || first_to_show<0) first_to_show=0;

	for(e=first_to_show; (e<pane_size && (e-first_to_show)<max_lines);e++)
	{

		if(list[e].size<2048) {e_size=list[e].size; sprintf(e_stype, "%s", "B ");}
		if(list[e].size>=2048 && list[e].size<2097152) {e_size=list[e].size/1024.f; sprintf(e_stype, "%s", "KB");} //KB
		if(list[e].size>=2097152 && list[e].size<2147483648U) {e_size=list[e].size/1048576.f; sprintf(e_stype, "%s", "MB");}//MB
		if(list[e].size>=2147483648U) {e_size=list[e].size/1073741824.f; sprintf(e_stype, "%s", "GB");} //GB

		strncpy(e_name, list[e].path, 10);
		if(strstr (e_name,"/net_host")!=NULL)
				sprintf(e_date, "%s", list[e].datetime);
		else {
		timeinfo = localtime ( &list[e].time );
		if(date_format==0) sprintf(e_date,"%02d/%02d/%04d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
		else if(date_format==1) sprintf(e_date, "%02d/%02d/%04d", timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_year+1900);
		else if(date_format==2) sprintf(e_date, "%04d/%02d/%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday );
		}

	//utf8_to_ansi(list[e].name, entry_name, 128);
	strncpy(entry_name, list[e].name, 128); entry_name[128]=0;
//	if(user_font<2)
//	{
		if(x_offset>=0.54f)
			{ sprintf(e_name,"%s", entry_name); e_name[55]=0;}
		else
			{ sprintf(e_name,"%s", entry_name); e_name[60]=0; }
//	}
//	else
//	{
//		if(x_offset>=0.54f)
//			{ sprintf(e_name,"%-24s", entry_name); e_name[24]=0;}
//		else
//			{ sprintf(e_name,"%-42s", entry_name); e_name[42]=0; }
//	}


		if(list[e].type==0) 
			sprintf(e_sizet, "%s", "  <dir>");
		else
		{
			if(e_stype[0]=='B')
				sprintf(e_sizet,"%.0f %s", e_size, e_stype); 
			else
				sprintf(e_sizet,"%.2f %s", e_size, e_stype); 
		}

		e_sizet[11]=0;
		//sprintf(str, "%s %s %s", e_name, e_sizet, e_date); //((list[e].type==0) ?  "D" : "F"),
		sprintf(str, "%s", e_name);
		sprintf(str_date, "%s", e_date);
		//str[49]=0;
		color=((list[e].type==0) ? (COL_FMDIR) : (COL_FMFILE));

		if(strstr(list[e].name, ".mp3")!=NULL || strstr(list[e].name, ".MP3")!=NULL) color=COL_FMMP3;
		else if(strstr(list[e].name, ".ac3")!=NULL || strstr(list[e].name, ".AC3")!=NULL || strstr(list[e].name, ".FLAC")!=NULL || strstr(list[e].name, ".flac")!=NULL) color=COL_FMMP3;
		else if(strstr(list[e].name, ".jpg")!=NULL || strstr(list[e].name, ".jpeg")!=NULL || strstr(list[e].name, ".png")!=NULL || strstr(list[e].name, ".JPG")!=NULL || strstr(list[e].name, ".JPEG")!=NULL || strstr(list[e].name, ".PNG")!=NULL) color=COL_FMJPG;//1133cc;
		else if(strstr(list[e].name, "EBOOT.BIN")!=NULL || strstr(list[e].name, "INDEX.BDM")!=NULL || strstr(list[e].name, "index.bdmv")!=NULL || strstr(list[e].name, ".self")!=NULL || strstr(list[e].name, ".SELF")!=NULL || strstr(list[e].name, ".pkg") || strstr(list[e].name, ".PKG")!=NULL) color=COL_FMEXE;
		else if(strstr(list[e].name, ".MMT")!=NULL || strstr(list[e].name, ".mmt")!=NULL) color=0xffe0d0c0;
		else if(is_video(list[e].name)) color=0xff1070f0;

		color=( (color & 0x00ffffff) | (c_opacity2<<24));

		if(list[e].selected)// && x_offset>=0.54f) //c_opacity2>0x20 && 
			{
				if(x_offset>=0.54f)
					draw_square((x_offset-0.015f-0.5f)*2.0f, (0.5f-y_offset)*2.0f, 0.82f, 0.048f, 0.5f, 0x1080ff30);
				else
					draw_square((x_offset-0.015f-0.5f)*2.0f, (0.5f-y_offset)*2.0f, 0.92f, 0.048f, 0.5f, 0x1080ff30);
			}

		if(mouseX>=x_offset && mouseX<=x_offset+0.430f && mouseY>=y_offset && mouseY<=y_offset+0.026f)
		{
			e_attributes2[0]=0;
			if(list[e].mode>0){                      //012 456 789
				sprintf(e_attributes, "%s", "--- --- ---");
				if(list[e].type==1) 
				{
					if(list[e].mode & S_IXOTH) e_attributes[0]='x';
					if(list[e].mode & S_IXGRP) e_attributes[4]='x';
					if(list[e].mode & S_IXUSR) e_attributes[8]='x';
				}
				else
				{
					e_attributes[0]='d'; 
					e_attributes[4]='d';
					e_attributes[8]='d';
				}


				if(list[e].mode & S_IWOTH) e_attributes[1]='w';
				if(list[e].mode & S_IROTH) e_attributes[2]='r';

				if(list[e].mode & S_IWGRP) e_attributes[5]='w';
				if(list[e].mode & S_IRGRP) e_attributes[6]='r';

				if(list[e].mode & S_IWUSR) e_attributes[9]='w';
				if(list[e].mode & S_IRUSR) e_attributes[10]='r';

				sprintf(e_attributes2,"  |  Attr: %s", e_attributes);
			}
			u32 select_color=0x0080ff60;
//			if(c_opacity2<0x80) { select_color=select_color & 0xffffff00 | c_opacity2;}
			//if(c_opacity2>0x20)
				{
					if(x_offset>=0.54f)
						draw_square((x_offset-0.015f-0.5f)*2.0f, (0.5f-y_offset)*2.0f, 0.82f, 0.048f, 0.5f, select_color);
					else
						draw_square((x_offset-0.015f-0.5f)*2.0f, (0.5f-y_offset)*2.0f, 0.92f, 0.048f, 0.5f, select_color);
				}
			if((strlen(list[e].name)>25 || strlen(e_attributes2)>0) && !(strlen(list[e].name)==2 && list[e].name[0]==0x2e && list[e].name[1]==0x2e) )
			{ //display hint
				if(list[e].type==0)
					sprintf(e_name,"Dir : %s", list[e].name);
				else
					sprintf(e_name,"File: %s", list[e].name);
				e_name[86]=0;
				cellDbgFontPrintf( 0.04f+new_offset, 0.895f, 0.7f , COL_HEXVIEW, e_name);
				sprintf(e_name,"Date: %s %s:%02d:%02d%s", e_date, tmhour(timeinfo->tm_hour), timeinfo->tm_min, timeinfo->tm_sec, e_attributes2);
				cellDbgFontPrintf( 0.04f+new_offset, 0.916f, 0.7f , COL_HEXVIEW, e_name);
			}
			//color=0xc01010a0;

			sprintf(fm_func, "%s", "none");
			if ((new_pad & BUTTON_CIRCLE)) 
			{
				int fmret=-1;
				int m_copy_total=0;
				for(int m_copy=0; m_copy<pane_size; m_copy++) m_copy_total+=list[m_copy].selected;
				if(m_copy_total>1)
				{
					if(list[e].type==0)
						fmret=context_menu((char*) "Multiple folders", list[e].type, this_pane, other_pane);
					else
						fmret=context_menu((char*) "Multiple files", list[e].type, this_pane, other_pane);
				}
				else
					fmret=context_menu(list[e].name, list[e].type, this_pane, other_pane);
				new_pad=0;
				if(fmret!=-1)
				{
					sprintf(fm_func, "%s", opt_list[fmret].value);
				}
			}

			if ( !strcmp(fm_func, "test") && viewer_open==0 ) 
			{
				sprintf(my_txt_file, "%s/%s", this_pane, list[e].name);
				sprintf(fm_func, "%s", "none");
				time_start= time(NULL);
					
				abort_copy=0;
				initConsole();
				file_counter=0;
				new_pad=0;
				global_device_bytes=0;
				num_directories= file_counter= num_files_big= num_files_split= 0;

				sprintf(string1,"Checking, please wait...\n\n%s", my_txt_file);

				cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
				draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
				cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
				cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Hold /\\ to Abort");
				cellDbgFontDrawGcm();
				flip(); 

				my_game_test( my_txt_file, 0);

				DPrintf("Directories: %i Files: %i\nBig files: %i Split files: %i\n\n", num_directories, file_counter, num_files_big, num_files_split);

				int seconds= (int) (time(NULL)-time_start);
				int vflip=0;

				while(1){

					if(abort_copy==2) sprintf(string1,"Aborted!  Time: %2.2i:%2.2i:%2.2i\n", seconds/3600, (seconds/60) % 60, seconds % 60);
					else
						if(abort_copy==1)
						sprintf(string1,"Folder contains over %i files. Time: %2.2i:%2.2i:%2.2i Vol: %1.2f GB+\n", file_counter, seconds/3600, (seconds/60) % 60, seconds % 60, ((double) global_device_bytes)/(1024.0*1024.*1024.0));
						else
						sprintf(string1,"Files tested: %i Time: %2.2i:%2.2i:%2.2i Size: %1.2f GB\nActual size : %.f bytes", file_counter, seconds/3600, (seconds/60) % 60, seconds % 60, ((double) global_device_bytes)/(1024.0*1024.*1024.0),(double) global_device_bytes);
					

					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
					cellDbgFontPrintf( 0.07f, 0.07f, 1.2f,0xc0c0c0c0,string1);
					
					if(vflip & 32)
					cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xffffffff, "Press [ ] to continue");
					vflip++;

					cellDbgFontDrawGcm();
					flip();
					pad_read();
					if (new_pad & BUTTON_SQUARE) 
						{
						new_pad=0;
						break;
						}

					}
				termConsole();
			}

//			if ( ((new_pad & BUTTON_R3) && (list[e].type==1)) && viewer_open==0 ) 
			if ( !strcmp(fm_func, "view") && viewer_open==0 ) 
			{
//fm HEX view
				sprintf(fm_func, "%s", "none");
				sprintf(my_txt_file, "%s", list[e].path);
				new_pad=0; old_pad=0;

				if(strstr(list[e].path,"/net_host")!=NULL) //network copy
				{
					char cpath[1024], cpath2[1024];
					int chost=0; int pl=strlen(list[e].path);
					chost=list[e].path[9]-0x30;
					for(int n=11;n<pl;n++)
					{cpath[n-11]=list[e].path[n]; cpath[n-10]=0;}
					sprintf(cpath2, "/%s", cpath); //host_list[chost].root,
					sprintf(my_txt_file, "%s/TEMP/hex_view.bin", app_usrdir);
					network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) my_txt_file, 0);
				}

				if(strstr(list[e].path,"/pvd_usb")!=NULL) //ntfs
				{
					sprintf(my_txt_file, "%s/TEMP/hex_view.bin", app_usrdir);
					file_copy(list[e].path, my_txt_file, 0);
				}
				viewer_open=1;
				//txt viewer

			uint64_t fsiz = 0, readb=0;
			uint64_t msiz = 0;
			uint64_t msiz1 = 0;
			uint64_t msiz2 = 0x800000;

			FILE *fp;
	
			unsigned int chunk = 512;
			int view_mode=0; // 0=file 1=mem

			fp = fopen(my_txt_file, "rb");
			if(fp==NULL) goto quit_viewer;

			fseek(fp, 0, SEEK_END);
			msiz=ftell(fp);
			msiz1=msiz;

			if(msiz<chunk && msiz>0) chunk=msiz;

			unsigned char x[chunk];

			fseek(fp, 0, SEEK_SET);
			readb=fread((void *) x, 1, chunk, fp);

			if(readb<1)
				goto close_viewer;
			else
				{
				memset(text_bmpUPSR, 0x50, V_WIDTH*V_HEIGHT*4);

				unsigned int li, chp, cfp, hp;
				char cchar[512];
				char clin[512];
				char clintxt[32];

				while(1)
				{
					pad_read();


					if ((old_pad & BUTTON_START) && view_mode==1) //dump lv2
					{
						//old_pad=0; new_pad=0;
						time ( &rawtime );
						timeinfo = localtime ( &rawtime );
						char lv2file[64];
						sprintf(lv2file, "/dev_hdd0/%04d%02d%02d-%02d%02d%02d-LV2-FW%1.2f.BIN", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, c_firmware);
						sprintf(string1, "Exporting GameOS memory to file:\n\n%s\n\nPlease wait...", lv2file);
						dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, string1, dialog_fun2, (void*)0x0000aaab, NULL );
						flipc(60);

						FILE *fpA;

						remove(lv2file);
						fpA = fopen ( lv2file, "wb" );
						readb=readmem((unsigned char *) text_FONT, 0, msiz2);
						fwrite(text_FONT, readb, 1, fpA);
						fclose(fpA);
						//load_texture(text_bmpBG, userBG, 1920);
						cellMsgDialogAbort();
						sprintf(string1, "GameOS memory exported successfully to file:\n\n%s", lv2file);
						dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
					}

					if ((new_pad & BUTTON_SELECT)) 
					{
						//old_pad=0;// 
						new_pad=0;
						view_mode = 1-view_mode;
						if(view_mode==0) {
							msiz=msiz1; 
							if(msiz<chunk && msiz>0) chunk=msiz;
							fsiz=0;
							fseek(fp, fsiz, SEEK_SET);
							readb=fread((void *) x, 1, chunk, fp);
							if(readb<1)	goto close_viewer;
						}
						else {
							msiz=msiz2;
							chunk=512;
							fsiz=0;
							readb=readmem((unsigned char *) x, fsiz, chunk);
						}
					}

				if (view_mode==1)  //view mem
				{
					if ((old_pad & BUTTON_L1)) 
					{
						old_pad=0;
						fsiz=0;
						readb=readmem((unsigned char *) x, fsiz, chunk);
						if(readb<1)	goto close_viewer;

					}

					if ((old_pad & BUTTON_R1) && msiz>=512) 
					{
						old_pad=0;
						fsiz=msiz-512;
						readb=readmem((unsigned char *) x, fsiz, chunk);
						if(readb<1)	goto close_viewer;
					}


					if ((old_pad & BUTTON_L2) && fsiz>=16) 
					{
						old_pad=0;
						fsiz-=16;
						readb=readmem((unsigned char *) x, fsiz, chunk);
						if(readb<1)	goto close_viewer;

					}

					if ((old_pad & BUTTON_R2) && ((fsiz+16)<msiz) )
					{
						old_pad=0;
						fsiz+=16;
						readb=readmem((unsigned char *) x, fsiz, chunk);
						if(readb<1)	goto close_viewer;
					}


					if ((old_pad & BUTTON_UP)) 
					{
						old_pad=0;
						if(fsiz>=512) fsiz-=512; else fsiz=0;
						readb=readmem((unsigned char *) x, fsiz, chunk);
						if(readb<1)	goto close_viewer;

					}

					if ((old_pad & BUTTON_DOWN) && ((fsiz+512)<msiz) )
					{
						old_pad=0;
						fsiz+=512;
						readb=readmem((unsigned char *) x, fsiz, chunk);
						if(readb<1)	goto close_viewer;
					}

					if ((old_pad & BUTTON_LEFT)) 
					{
						old_pad=0;
						if(fsiz>=8192) fsiz-=8192; else fsiz=0;
						readb=readmem((unsigned char *) x, fsiz, chunk);
						if(readb<1)	goto close_viewer;

					}

					if ((old_pad & BUTTON_RIGHT) && ( (fsiz+8192)<msiz) )
					{
						old_pad=0;
						fsiz+=8192;
						readb=readmem((unsigned char *) x, fsiz, chunk);
						if(readb<1)	goto close_viewer;
					}

				} //view mem



				if (view_mode==0) //view file
				{
					if ((old_pad & BUTTON_L1)) 
					{
						old_pad=0;
						fsiz=0;
						fseek(fp, fsiz, SEEK_SET);
						readb=fread((void *) x, 1, chunk, fp);
						if(readb<1)	goto close_viewer;

					}

					if ((old_pad & BUTTON_R1) && msiz>=512) 
					{
						old_pad=0;
						fsiz=msiz-512;
						fseek(fp, fsiz, SEEK_SET);
						readb=fread((void *) x, 1, chunk, fp);
						if(readb<1)	goto close_viewer;
					}


					if ((old_pad & BUTTON_L2) && fsiz>=16) 
					{
						old_pad=0;
						fsiz-=16;
						fseek(fp, fsiz, SEEK_SET);
						readb=fread((void *) x, 1, chunk, fp);
						if(readb<1)	goto close_viewer;

					}

					if ((old_pad & BUTTON_R2) && ((fsiz+16)<msiz) )
					{
						old_pad=0;
						fsiz+=16;
						fseek(fp, fsiz, SEEK_SET);
						readb=fread((void *) x, 1, chunk, fp);
						if(readb<1)	goto close_viewer;
					}


					if ((old_pad & BUTTON_UP)) 
					{
						old_pad=0;
						if(fsiz>=512) fsiz-=512; else fsiz=0;
						fseek(fp, fsiz, SEEK_SET);
						readb=fread((void *) x, 1, chunk, fp);
						if(readb<1)	goto close_viewer;

					}

					if ((old_pad & BUTTON_DOWN) && ((fsiz+512)<msiz) )
					{
						old_pad=0;
						fsiz+=512;
						fseek(fp, fsiz, SEEK_SET);
						readb=fread((void *) x, 1, chunk, fp);
						if(readb<1)	goto close_viewer;
					}

					if ((old_pad & BUTTON_LEFT)) 
					{
						old_pad=0;
						if(fsiz>=8192) fsiz-=8192; else fsiz=0;
						fseek(fp, fsiz, SEEK_SET);
						readb=fread((void *) x, 1, chunk, fp);
						if(readb<1)	goto close_viewer;

					}

					if ((old_pad & BUTTON_RIGHT) && ( (fsiz+8192)<msiz) )
					{
						old_pad=0;
						fsiz+=8192;
						fseek(fp, fsiz, SEEK_SET);
						readb=fread((void *) x, 1, chunk, fp);
						if(readb<1)	goto close_viewer;
					}

				}

					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					mouseX+=mouseXD; mouseY+=mouseYD;
					if(mouseX>0.995f) {mouseX=0.995f;mouseXD=0.0f;} if(mouseX<0.0f) {mouseX=0.0f;mouseXD=0.0f;}
					if(mouseY>0.990f) {mouseY=0.990f;mouseYD=0.0f;} if(mouseY<0.0f) {mouseY=0.0f;mouseYD=0.0f;}
//					draw_triangle((mouseX-0.5f)*2.0f, (0.5f-mouseY)*2.0f, 0.02f, 0.035f, -1.0f, 0x2080ffff);
//					draw_triangle((mouseX-0.0011f-0.5f)*2.0f, (0.5f-mouseY+0.0015f)*2.0f, 0.0258f, 0.043f, -1.0f, 0xd0d0d0ff);

				time ( &rawtime );
				timeinfo = localtime ( &rawtime );
				if(date_format==0) sprintf(string1, "%02d/%02d/%04d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
				else if(date_format==1) sprintf(string1, "%02d/%02d/%04d", timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_year+1900);
				else if(date_format==2) sprintf(string1, "%04d/%02d/%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday );
				cellDbgFontPrintf( 0.83f, 0.895f, 0.7f , COL_HEXVIEW, "%s\n %s:%02d:%02d ", string1, tmhour(timeinfo->tm_hour), timeinfo->tm_min, timeinfo->tm_sec);

				if(view_mode==1)
					sprintf(e_name,"GameOS memory  |  Press [START] to export GameOS memory to file");
				else
					sprintf(e_name,"File: %s", list[e].name);
				e_name[86]=0;
				cellDbgFontPrintf( 0.04f+new_offset, 0.895f, 0.7f , COL_HEXVIEW, e_name);
				if(view_mode==1)
					sprintf(e_name,"Memory offset: 0x%08X / 0x%08X | [SELECT] for file view", (int)fsiz, (int)msiz);
				else
					sprintf(e_name,"Offset: 0x%X (%.0f) / 0x%X (%.0f) | [SELECT] for LV2 view", (int)fsiz, (double)fsiz, (int)msiz, (double)msiz); //Date: %s |  e_date,
				cellDbgFontPrintf( 0.04f+new_offset, 0.916f, 0.7f , COL_HEXVIEW, e_name);

				for(li=0; li<32; li++)
				{
					chp=0;
					sprintf(clin, "%-48s |  -16%s", " ", " "); 
					clin[0]=0;
					clintxt[0]=0;
					clintxt[16]=0;
					for(hp=0; hp<16; hp++)
					{
						cfp = li*16+hp;
						clintxt[hp]=0;
						clin[chp]=0;
						clin[chp+(hp*3)]=0;
						if( (cfp) < readb)
						{
							sprintf(cchar, "%02X", x[cfp]);
							if(x[cfp]>0x19 && x[cfp]<0x7f) clintxt[hp]=x[cfp]; else clintxt[hp]=0x2e;
							clin[chp+0]=cchar[0];
							clin[chp+1]=cchar[1];
							clin[chp+2]=0x20;
							clin[chp+3]=0x00;
							chp+=3;
						}
						else break;
					}
					if(strlen(clintxt)==0 || chp==0) break;
					sprintf(clin, "%-48s |  %-16s", clin, clintxt);  
					if(mouseX>=0.11f && mouseX<=0.89f && mouseY>=(0.07f+li*0.024f) && mouseY<=(0.07f+li*0.024f)+0.026f)
					{
						draw_square((0.11f+new_offset-0.015f-0.5f)*2.0f, (0.5f-(0.07f+li*0.024f))*2.0f, 1.52f, 0.048f, 0.0f, 0x0080ff80);
						select_color=0xc0e0e0e0;
					}
					else
						select_color=COL_HEXVIEW;
					cellDbgFontPrintf( 0.11f+new_offset, 0.07f+li*0.024f, 0.7f ,select_color, "0x%04X%04X: %s  |", (unsigned int)((fsiz+li*16)/0x10000), (unsigned int)((fsiz+li*16)%0x10000), clin, clintxt);
				}


				set_texture( text_FMS, 1920, 48); 
//				display_img(0, 47, 1920, 60, 1920, 48, -0.15f);
				display_img(0, 952, 1920, 76, 1920, 48, -0.15f, 1920, 48);

				if(animation==2 || animation==3) {
					BoffX-=1;
					if(BoffX<= -3840) BoffX=0;
					set_texture( text_bmpUPSR, 1920, 1080); 
					

					if(BoffX>= -1920) {
						display_img((int)BoffX, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					}

					display_img(1920+(int)BoffX, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);

					if(BoffX<= -1920) {
						display_img(3840+(int)BoffX, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					}

					}

				else
					{
						set_texture( text_bmpUPSR, 1920, 1080); 
						
						display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					}

					cellDbgFontDrawGcm();
					draw_mouse_pointer(0);					
					flip();

					//cellSysutilCheckCallback();

					if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE)) break;

				}
				}
close_viewer:
				fclose(fp);
quit_viewer:
				viewer_open=0;
				new_pad=0; old_pad=0;
				state_draw=1;
				state_read=1;

			}


//			if ( ((new_pad & BUTTON_L3) || osk_open==2) && strstr(this_pane,"/ps3_home")==NULL)
			if ( (!strcmp(fm_func, "newfolder") || osk_open==2) )// && strstr(this_pane,"/ps3_home")==NULL)
			{
//fm new folder
				new_pad=0; old_pad=0;

				if(osk_open!=2)	{
					OutputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK; /* Result on-screen keyboard dialog termination */ 
					OutputInfo.numCharsResultString = 128;	/* Specify number of characters for returned text */
					OutputInfo.pResultString = Result_Text_Buffer;   /* Buffer storing returned text */ ;
					open_osk(2, (char*) list[e].name);
				}

				if(osk_dialog!=0)
				{
					sprintf(new_file_name, "%S", (wchar_t*)OutputInfo.pResultString);
					if(strlen(new_file_name)>0) {
						sprintf(new_file_name, "%s/%S", this_pane, (wchar_t*)OutputInfo.pResultString);

						if(strstr(this_pane, "/net_host")!=NULL){

							char cpath2[1024];
							int chost=0;
							chost=this_pane[9]-0x30;
							if(this_pane[10]==0)
								sprintf(cpath2, "/%S/",  (wchar_t*)OutputInfo.pResultString);
							else
								sprintf(cpath2, "/%s/%S/", this_pane+11, (wchar_t*)OutputInfo.pResultString);
							network_del((char*)"PUT", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) "blank", 3);
							network_com((char*)"GET!",(char*)host_list[chost].host, host_list[chost].port, (char*)"/", (char*) host_list[chost].name, 1);
						}
						else
							mkdir(new_file_name, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(new_file_name, 0777);

						if(x_offset>=0.54f) state_read=3; else state_read=2;
					}
				osk_open=0;
				}

			}

//			if ( ((old_pad & BUTTON_SELECT) && (new_pad & BUTTON_CROSS) && (list[e].name[0]!=0x2e) && strlen(list[e].path)>8 && strstr(this_pane, "/ps3_home")==NULL) || osk_open==1)
			if ( (!strcmp(fm_func, "nethost")))
			{
				int n=0; // if SELECT-X for network file - perform HOST file update
				n=list[e].path[9]-0x30;
				network_com((char*)"GET!", (char*)host_list[n].host, host_list[n].port, (char*)"/", (char*) host_list[n].name, 1);//host_list[n].root
				if(x_offset>=0.54f) state_read=3; else state_read=2;
			}

			if (!strcmp(fm_func, "rename") || osk_open==1)
			{
//fm rename
				join_copy=0;
				new_pad=0; old_pad=0;

//osk_rename:
				if(osk_open!=1)	{
					OutputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK; /* Result on-screen keyboard dialog termination */ 
					OutputInfo.numCharsResultString = 128;	/* Specify number of characters for returned text */
					OutputInfo.pResultString = Result_Text_Buffer;   /* Buffer storing returned text */ ;
					open_osk(1, (char*) list[e].name);
				}

				if(osk_dialog!=0)
				{
					sprintf(new_file_name, "%S", (wchar_t*)OutputInfo.pResultString);
					if(strlen(new_file_name)>0) {
						sprintf(new_file_name, "%s/%S", this_pane, (wchar_t*)OutputInfo.pResultString);
						rename(list[e].path, new_file_name);
						if(x_offset>=0.54f) state_read=3; else state_read=2;
						if(!strcmp(this_pane, other_pane)) state_read=1;
					}
				osk_open=0;
				}
//osk_end:

			}



			if (((new_pad & BUTTON_CROSS) && (list[e].type==0)) || ((new_pad & BUTTON_TRIANGLE) && strcmp(list[0].path, "/app_home")))
			{
//open_folder:
				if((new_pad & BUTTON_TRIANGLE)) e=0;

				first_to_show=0;
				mouseY=0.12f+new_offset+0.013f; //move mouse to top of folder list

				if(strstr (list[e].path,"/net_host")!=NULL)
				{
					int n=list[e].path[9]-0x30; struct stat s;
					if(stat(host_list[n].name, &s)<0)
					network_com((char*)"GET", (char*)host_list[n].host, host_list[n].port, (char*)"/", (char*) host_list[n].name, 1);//host_list[n].root
				}

				state_draw=1;

				if(x_offset>=0.54f)
				{	first_right=0;
					sprintf(current_right_pane, "%s", list[e].path);
					state_read=3;
				}
				else
				{	first_left=0;
					sprintf(current_left_pane, "%s", list[e].path);
					state_read=2;
				}

				new_pad=0; old_pad=0; 
				
			}


			if (( (new_pad & BUTTON_CROSS)  || !strcmp(fm_func, "view")) && (list[e].type==1)) 
			{
//fm execute view
				sprintf(fm_func, "%s", "none");
				join_copy=0;
				new_pad=0;
				sprintf(my_mp3_file, "%s", list[e].path);
				char just_path[256], just_title[128], just_title_id[16];

				if(strstr(list[e].name, ".CNF")!=NULL || strstr(list[e].name, ".cnf")!=NULL)
				{
					
					strncpy(just_path, list[e].path, 11); just_path[11]=0;
					mod_mount_table(just_path, 1);
					dialog_ret=0;
					cellMsgDialogOpen2( type_dialog_ok, "multiMAN will now exit to XMB\xE2\x84\xA2 and you can start the game from the [PLAYSTATION\xC2\xAE\x32 Format Game] icon.", dialog_fun2, (void*)0x0000aaab, NULL ); 
					wait_dialog();
					unload_modules(); sys_process_exit(1);
				}

				if(strstr(list[e].name, ".pkg")!=NULL || strstr(list[e].name, ".PKG")!=NULL)
				{
					
					syscall_mount(this_pane, mount_bdvd);
					dialog_ret=0;
					sprintf(string1, "Do you want to exit to XMB\xE2\x84\xA2 to install selected package from [* Install package files] menu?"); 
					cellMsgDialogOpen2( type_dialog_yes_no, string1, dialog_fun1, (void*)0x0000aaaa, NULL );
					wait_dialog(); 
					if(dialog_ret!=1) {reset_mount_points(); goto cancel_exit;}
					unload_modules(); sys_process_exit(1);
				}

				if(strstr(my_mp3_file, "/USRDIR/EBOOT.BIN")!=NULL && strstr(my_mp3_file, "/net_host")==NULL)
				{
					sprintf(just_path, "%s", my_mp3_file);
					pch=just_path;
					if(strstr(my_mp3_file, "/PS3_GAME/USRDIR/EBOOT.BIN")!=NULL)
					{
						char *pathpos=strstr(pch,"/PS3_GAME/USRDIR/EBOOT.BIN");	
						just_path[pathpos-pch]=0;
						sprintf(filename, "%s/PS3_GAME/PARAM.SFO", just_path);
					}
					else
					{
						char *pathpos=strstr(pch,"/USRDIR/EBOOT.BIN");	
						just_path[pathpos-pch]=0;
						sprintf(filename, "%s/PARAM.SFO", just_path);
					}

					change_param_sfo_version(filename); int plevel=0;
					parse_param_sfo(filename, just_title, just_title_id, &plevel);
					num_files_split=0; abort_copy=0;
					my_game_test(just_path, 2); 
					if(num_files_split && payload!=1){
						sprintf(filename, "You cannot launch games with split big files!\n\nTransfer the game to internal HDD and try again\nor use XMMB mode to launch the title.");
						dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
						goto cancel_exit;
					}

		if(parental_level<plevel && parental_level>0)
		{
			sprintf(string1, "Game parental level: %i - Enter access PIN code:", plevel );

				OutputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
				OutputInfo.numCharsResultString = 128;
				OutputInfo.pResultString = Result_Text_Buffer;
				open_osk(3, (char*) string1);

				while(1){ 
					sprintf(string1, "::: %s :::\n\n\nSelected game is restricted with parental level %i.\n\nPlease enter four alphanumeric parental password code:", just_title, plevel);
					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.9f, 0xd0000080);
					cellDbgFontPrintf( 0.10f, 0.10f, 1.0f, 0xffffffff, string1);
					setRenderColor();
					cellDbgFontDrawGcm();
					flip(); 
					//cellSysutilCheckCallback();
					if(osk_dialog==1 || osk_dialog==-1) break;
					}
				cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
				flip(); cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
				flipc(60);
				osk_open=0;
				if(osk_dialog!=0)
				{
					char pin_result[32];
					wchar_t *pin_result2;
					pin_result2 = (wchar_t*)OutputInfo.pResultString;
					wcstombs(pin_result, pin_result2, 4);
					if(strlen(pin_result)==4) {
						if(strcmp(pin_result, parental_pass)==0) {
							goto pass_ok_2;
						}
					}
				}
				dialog_ret=0;
				cellMsgDialogOpen2( type_dialog_ok, "Entered PIN code is incorrect!", dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();
				goto cancel_exit;

		}

pass_ok_2:


					if(payload==0 && sc36_path_patch==0 && stat((char*)"/dev_bdvd", &s3)<0)
					{
						dialog_ret=0;
						cellMsgDialogOpen2( type_dialog_ok, "Please insert an original PLAYSTATION\xC2\xAE\x33 game disc before proceeding!", dialog_fun2, (void*)0x0000aaab, NULL );					
						wait_dialog();
					}
					else
						if(payload==0 && sc36_path_patch==1 && stat((char*)"/dev_bdvd", &s3)<0)
						{
							//dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, "Start your game from [* /app_home] menu.\n\nShould you run into problems - insert an original Playstation(R)3 game disc next time!", dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
							poke_sc36_path( (char *) "/app_home" );
						}


					dialog_ret=0;
					if(direct_launch==1) 
					{	sprintf(string1, "Do you want to start the game without exiting to XMB?\n\nWarning: Some games do not support such launch mode!"); 
						cellMsgDialogOpen2( type_dialog_yes_no, string1, dialog_fun1, (void*)0x0000aaaa, NULL );
						wait_dialog(); 
					}
					if(dialog_ret==3) {reset_mount_points(); goto cancel_exit;}

					if(dialog_ret==1)  
					{
						write_last_play( my_mp3_file, just_path, just_title, (char *) just_title_id, 1);
						unload_modules();
//						if(payload==0)	
						syscall_mount(just_path, mount_bdvd);
						sys_game_process_exitspawn2((char *) my_mp3_file, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_128K);
//						else sys_game_process_exitspawn2((char *) "/app_home/PS3_GAME/USRDIR/EBOOT.BIN", NULL, NULL, NULL, 0, 3071, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
					}

					write_last_play( my_mp3_file, just_path, just_title, (char *) just_title_id, 0);
					unload_modules();	
					syscall_mount(just_path, mount_bdvd);
					sys_process_exit(1);
					break;					
				}

				else if( (strstr(my_mp3_file, "/BDMV/INDEX.BDM")!=NULL || strstr(my_mp3_file, "/BDMV/index.bdmv")!=NULL) && strstr(my_mp3_file, "/net_host")==NULL)// && strstr(my_mp3_file, "/dev_hdd0")!=NULL)
				{
					sprintf(just_path, "%s", my_mp3_file);
					pch=just_path;
					char *pathpos;

					if(strstr(my_mp3_file, "/BDMV/INDEX.BDM")!=NULL)
					{
						pathpos=strstr(pch,"/BDMV/INDEX.BDM");	
						just_path[pathpos-pch]=0;
					}

					else if(strstr(my_mp3_file, "/BDMV/index.bdmv")!=NULL)
					{
						pathpos=strstr(pch,"/BDMV/index.bdmv");	
						just_path[pathpos-pch]=0;

			sprintf(filename, "::: %s :::\n\nDo you want to convert the Blu-ray\xE2\x84\xA2 disc structure to AVCHD\xE2\x84\xA2 format?\n\nNote: The action may improve playback compatibility!", just_path);
			dialog_ret=0;
			cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaab, NULL );
			wait_dialog();
			if(dialog_ret==1)  
			{	
				struct stat s2;
				dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Converting Blu-ray\xE2\x84\xA2 structure to AVCHD\xE2\x84\xA2, please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
				flipc(60);

				DIR  *dir;
				char path[512], cfile[512], ffile[512], cfile0[16];int n;
				for(n=0;n<64;n++){
				sprintf(path, "%s/BDMV/CLIPINF", just_path); dir=opendir (path);	while(dir) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".clpi")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.CPI", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); remove(cfile); rename(ffile, cfile);}}closedir(dir);
				
				sprintf(path, "%s/BDMV/BACKUP/CLIPINF", just_path); dir=opendir (path);	while(dir) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".clpi")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.CPI", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); remove(cfile); rename(ffile, cfile);}}closedir(dir);

				sprintf(path, "%s/BDMV/PLAYLIST", just_path); dir=opendir (path);	while(dir) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".mpls")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.MPL", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); remove(cfile); rename(ffile, cfile);}}closedir(dir);
				sprintf(path, "%s/BDMV/BACKUP/PLAYLIST", just_path); dir=opendir (path);	while(1) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".mpls")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.MPL", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); remove(cfile); rename(ffile, cfile);}}closedir(dir);

				sprintf(path, "%s/BDMV/STREAM", just_path); dir=opendir (path);	while(dir) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".m2ts")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.MTS", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); remove(cfile); rename(ffile, cfile);}}closedir(dir);

				sprintf(path, "%s/BDMV/STREAM/SSIF", just_path); dir=opendir (path);	while(dir) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".ssif")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.SSI", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); remove(cfile); rename(ffile, cfile);}}closedir(dir);
				}

				sprintf(path, "%s/BDMV/index.bdmv", just_path);	if(stat(path, &s2)>=0) {sprintf(cfile, "%s/BDMV/INDEX.BDM", just_path); remove(cfile); rename(path, cfile);}
				sprintf(path, "%s/BDMV/BACKUP/index.bdmv", just_path);	if(stat(path, &s2)>=0) {sprintf(cfile, "%s/BDMV/BACKUP/INDEX.BDM", just_path); remove(cfile); rename(path, cfile);}

				sprintf(path, "%s/BDMV/MovieObject.bdmv", just_path); if(stat(path, &s2)>=0) {sprintf(cfile, "%s/BDMV/MOVIEOBJ.BDM", just_path); remove(cfile); rename(path, cfile);}
				sprintf(path, "%s/BDMV/BACKUP/MovieObject.bdmv", just_path); if(stat(path, &s2)>=0) {sprintf(cfile, "%s/BDMV/BACKUP/MOVIEOBJ.BDM", just_path); remove(cfile); rename(path, cfile);}
				cellMsgDialogAbort();
			}
			else goto skip_BD;

					}

					dialog_ret=0;
					sprintf(string1, "Activate currently selected AVCHD\xE2\x84\xA2 video folder?\n\n::: %s :::\n\nNote: You can start video playback from XMB\xE2\x84\xA2 [Video] tab", just_path);
					cellMsgDialogOpen2( type_dialog_yes_no, string1, dialog_fun1, (void*)0x0000aaaa, NULL );
					wait_dialog(); 
					if(dialog_ret==1)  
					{

		dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Activating AVCHD\xE2\x84\xA2 structure, please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
		flipc(60);

		char usb_save[32]="/none"; usb_save[5]=0;


		sprintf(filename, "/dev_sd");
		if(stat(filename, &s3)>=0) {
				sprintf(usb_save, "/dev_sd/PRIVATE");
				 if(stat(usb_save, &s3)<0) mkdir(usb_save, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
		}

		if(stat(usb_save, &s3)<0) {
			sprintf(filename, "/dev_ms");
			if(stat(filename, &s3)>=0) {
					sprintf(usb_save, "/dev_ms");
			}
		}

		if(stat(usb_save, &s3)<0) {
			for(int n=0;n<9;n++){
				sprintf(filename, "/dev_usb00%i", n);
				if(stat(filename, &s3)>=0) {
					sprintf(usb_save, "%s", filename);
					break;
				}
			}
		}

	if(stat(usb_save, &s3)>=0) {

		sprintf(filename, "%s/AVCHD", usb_save); if(stat(filename, &s3)<0) mkdir(filename, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
		sprintf(filename, "%s/AVCHD/BDMV", usb_save); if(stat(filename, &s3)<0) mkdir(filename, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

		sprintf(filename, "%s/AVCHD/BDMV/INDEX.BDM", usb_save); if(stat(filename, &s3)<0) file_copy((char *) avchdIN, (char *) filename, 0);
		sprintf(filename, "%s/AVCHD/BDMV/MOVIEOBJ.BDM", usb_save); if(stat(filename, &s3)<0) file_copy((char *) avchdMV, (char *) filename, 0);

		sprintf(filename, "%s/AVCHD", usb_save);
		sprintf(usb_save, "%s", filename);

		cellMsgDialogAbort();
		syscall_mount2((char *)usb_save, (char *)just_path);
		unload_modules();
		sys_process_exit(1); break;

	}
	else
	{
		cellMsgDialogAbort();
		dialog_ret=0;
		cellMsgDialogOpen2( type_dialog_ok, "Please attach USB, SDHC or MemoryStick\xE2\x84\xA2 storage device to activate AVCHD\xE2\x84\xA2 HDD playback!", dialog_fun2, (void*)0x0000aaab, NULL );
		wait_dialog();
	}

					}
				}
		else if( (is_video(my_mp3_file) || 
				strstr(my_mp3_file, ".self")!=NULL || strstr(my_mp3_file, ".SELF")!=NULL || 
				strstr(my_mp3_file, ".FLAC")!=NULL || strstr(my_mp3_file, ".flac")!=NULL || 
				strstr(my_mp3_file, ".DTS")!=NULL || strstr(my_mp3_file, ".dts")!=NULL || 
				strstr(my_mp3_file, "EBOOT.BIN")!=NULL) 
					&& strstr(my_mp3_file, "/net_host")==NULL && strstr(my_mp3_file, ".jpg")==NULL && strstr(my_mp3_file, ".JPG")==NULL
				    && net_used_ignore() )
				{
					if(strstr(my_mp3_file, ".SELF")==NULL && strstr(my_mp3_file, ".self")==NULL && strstr(my_mp3_file, "EBOOT.BIN")==NULL)
					{
retry_showtime:
						sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
						if(stat(filename, &s3)>=0)
						{
							
							FILE *flist; 
							sprintf(string1, "%s/TEMP/SHOWTIME.TXT", app_usrdir);
							remove(string1);
							flist = fopen(string1, "w");
							sprintf(filename, "file://%s", my_mp3_file);fputs (filename,  flist ); 
							fclose(flist);
//							sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
//							sys_game_process_exitspawn2((char *) filename, NULL, NULL, NULL, 0, 3070, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
							cellFsGetFreeSize(app_usrdir, &blockSize, &freeSize);
							freeSpace = ( ((uint64_t)blockSize * freeSize));
							if(strstr(my_mp3_file,"/pvd_usb")!=NULL && (uint64_t)list[e].size<freeSpace) { // && stat((char*)"/dev_hdd1", &s3)>=0 && 
								sprintf(string1, "%s", "Caching file to internal temporary folder...");
								cellMsgDialogOpen2(	CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL	|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE,string1,NULL,NULL,NULL);
								cellFsGetFreeSize((char*)"/dev_hdd1", &blockSize, &freeSize); freeSpace = ( ((uint64_t)blockSize * freeSize));
								if((uint64_t)list[e].size<freeSpace)
								{
									sprintf(filename, "%s", "/dev_hdd1/multiMAN");
									file_copy(my_mp3_file, filename, 1);

									if(my_mp3_file[strlen(my_mp3_file)-4]=='.') my_mp3_file[strlen(my_mp3_file)-4]=0;
									else if(my_mp3_file[strlen(my_mp3_file)-5]=='.') my_mp3_file[strlen(my_mp3_file)-5]=0;
									char imgfile2[512];

									sprintf(imgfile2, "%s.srt", my_mp3_file); {sprintf(filename, "%s", "/dev_hdd1/multiMAN.srt");file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.SRT", my_mp3_file); {sprintf(filename, "%s", "/dev_hdd1/multiMAN.srt");file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.ssa", my_mp3_file); {sprintf(filename, "%s", "/dev_hdd1/multiMAN.ssa");file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.SSA", my_mp3_file); {sprintf(filename, "%s", "/dev_hdd1/multiMAN.ssa");file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.ass", my_mp3_file); {sprintf(filename, "%s", "/dev_hdd1/multiMAN.ass");file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.ASS", my_mp3_file); {sprintf(filename, "%s", "/dev_hdd1/multiMAN.ass");file_copy(imgfile2, filename, 0);}
									sprintf(my_mp3_file, "%s", "/dev_hdd1/multiMAN");
								}
								else
								{
									sprintf(filename, "%s/TEMP/multiMAN", app_usrdir);
									file_copy(my_mp3_file, filename, 1);

									if(my_mp3_file[strlen(my_mp3_file)-4]=='.') my_mp3_file[strlen(my_mp3_file)-4]=0;
									else if(my_mp3_file[strlen(my_mp3_file)-5]=='.') my_mp3_file[strlen(my_mp3_file)-5]=0;
									char imgfile2[512];

									sprintf(imgfile2, "%s.srt", my_mp3_file); {sprintf(filename, "%s/TEMP/multiMAN.srt", app_usrdir);file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.SRT", my_mp3_file); {sprintf(filename, "%s/TEMP/multiMAN.srt", app_usrdir);file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.ssa", my_mp3_file); {sprintf(filename, "%s/TEMP/multiMAN.ssa", app_usrdir);file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.SSA", my_mp3_file); {sprintf(filename, "%s/TEMP/multiMAN.ssa", app_usrdir);file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.ass", my_mp3_file); {sprintf(filename, "%s/TEMP/multiMAN.ass", app_usrdir);file_copy(imgfile2, filename, 0);}
									sprintf(imgfile2, "%s.ASS", my_mp3_file); {sprintf(filename, "%s/TEMP/multiMAN.ass", app_usrdir);file_copy(imgfile2, filename, 0);}
									sprintf(my_mp3_file, "%s/TEMP/multiMAN", app_usrdir);
								}
								cellMsgDialogAbort();
								sprintf(string1, "%s/TEMP/SHOWTIME.TXT", app_usrdir);
								remove(string1);
								flist = fopen(string1, "w");
								sprintf(filename, "file://%s", my_mp3_file);fputs (filename,  flist ); 
								fclose(flist);

							}
							if(stat(my_mp3_file, &s3)<0)
							{
								cellMsgDialogAbort();
								dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, "Your current configuration doesn't support this function!\n\n                     (unable to create file cache)", dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();

							}
							else
							{
/*								pad_read();
								char* launchenvp[2];
								char* launchargv[1];
								memset(launchenvp, 0, sizeof(launchenvp));
								memset(launchargv, 0, sizeof(launchargv));
								launchenvp[0] = (char*)malloc(0x440);
								snprintf(launchenvp[0], 0x440, "showtime=%s", my_mp3_file);
								char args[512]; sprintf(args, "file://%s", my_mp3_file);
								int argslen=strlen(args)+1;

								int pos = 0;
								int i = 0;
								while (pos < argslen) {
									int len = strlen((char*)args + pos);
									if (!len)
										break;
									launchargv[i] = (char*)malloc(len + 1);
									strcpy(launchargv[i], (char*)args + pos);
									pos += len + 1;
									i++;
								} */
								unload_modules();
								sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
								sys_game_process_exitspawn2(filename, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
//								sys_game_process_exitspawn2(filename, (const char**)launchargv, (const char**)launchenvp, NULL, 0, 1001, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
							}
						}
						else
						{
							cellMsgDialogAbort();
							dialog_ret=0;
							sprintf(filename, "Showtime for multiMAN application is missing!\n\nDo you want to download it now?");
							cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
							wait_dialog();
							if(dialog_ret==1)
							{
								sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
								sprintf(string1, "%s/SHOWTIME.SELF", url_base);
								download_file(string1, filename, 1);
								goto retry_showtime;
							}
						}
					}
					else
					{
						unload_modules();
						sys_game_process_exitspawn2((char *) my_mp3_file, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_128K);
						sys_process_exit(1);
					}
				}



		else if(is_snes9x(my_mp3_file) && strstr(my_mp3_file, "/net_host")==NULL)
				{
					launch_snes_emu(my_mp3_file);
				}

		else if(is_genp(my_mp3_file) && strstr(my_mp3_file, "/net_host")==NULL)
				{
					launch_genp_emu(my_mp3_file);
				}

		else if(is_fceu(my_mp3_file) && strstr(my_mp3_file, "/net_host")==NULL)
				{
					launch_fceu_emu(my_mp3_file);
				}

		else if(is_vba(my_mp3_file) && strstr(my_mp3_file, "/net_host")==NULL)
				{
					launch_vba_emu(my_mp3_file);
				}

		else if(strstr(my_mp3_file, ".mmt")!=NULL || strstr(my_mp3_file, ".MMT")!=NULL && strstr(my_mp3_file, "/net_host")==NULL)
				{
					apply_theme(my_mp3_file, this_pane);
				}


skip_BD:
				if(strstr(my_mp3_file, ".mp3")!=NULL || strstr(my_mp3_file, ".MP3")!=NULL)
				{
						if(strstr(list[e].path,"/net_host")!=NULL) //network copy
						{

							char cpath[1024], cpath2[1024];
							int chost=0; int pl=strlen(list[e].path);
							chost=list[e].path[9]-0x30;
							for(int n=11;n<pl;n++)
							{cpath[n-11]=list[e].path[n]; cpath[n-10]=0;}
							sprintf(cpath2, "/%s", cpath); //host_list[chost].root,
							sprintf(my_mp3_file, "%s/TEMP/%s", app_usrdir, list[e].name);
							network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) my_mp3_file, 0);
							main_mp3((char*) my_mp3_file);
							max_mp3=0;
							current_mp3=0;
							remove(my_mp3_file);
						}
					else
						{
							int ci2=e+1;
							max_mp3=1;
							current_mp3=1;
							sprintf(mp3_playlist[max_mp3].path, "%s", my_mp3_file);
							
							//add the rest of the files as a playlist
							for(ci2=e+1; ci2<pane_size; ci2++)
							{
								sprintf(my_mp3_file, "%s", list[ci2].path);
								if(strstr(my_mp3_file, ".mp3")!=NULL || strstr(my_mp3_file, ".MP3")!=NULL)
								{
									if(max_mp3>=MAX_MP3) break;
									max_mp3++;
									sprintf(mp3_playlist[max_mp3].path, "%s", my_mp3_file);

								}

							}

							for(ci2=1; ci2<(e-1); ci2++)
							{
								sprintf(my_mp3_file, "%s", list[ci2].path);
								if(strstr(my_mp3_file, ".mp3")!=NULL || strstr(my_mp3_file, ".MP3")!=NULL)
								{
									if(max_mp3>=MAX_MP3) break;
									max_mp3++;
									sprintf(mp3_playlist[max_mp3].path, "%s", my_mp3_file);
								}

							}
							main_mp3((char*) mp3_playlist[1].path);
						}

				}

				sprintf(my_mp3_file, "%s", list[e].path);

			int current_image=e;
			long slide_time=0;
			int slide_show=0;
			int show_info=0;
			sprintf(my_mp3_file, "%s", list[current_image].path);

			if(strstr(my_mp3_file, ".jpg")!=NULL || strstr(my_mp3_file, ".JPG")!=NULL || strstr(my_mp3_file, ".jpeg")!=NULL || strstr(my_mp3_file, ".JPEG")!=NULL || strstr(my_mp3_file, ".png")!=NULL || strstr(my_mp3_file, ".PNG")!=NULL)
				{
			int to_break=0, slide_dir=0;
			float pic_zoom=1.0f;
			int	pic_reload=1, pic_posY=0, pic_posX=0, pic_X=0, pic_Y=0;
			char pic_info[512];
			use_analog=1;
			mouseYDR=mouseXDR=mouseYDL=mouseXDL=0.0000f;
			while(1)
				{ // Picture Viewer Mode
				//cellSysutilCheckCallback();
				sprintf(my_mp3_file, "%s", list[current_image].path);
				if(strstr(my_mp3_file, ".jpg")!=NULL || strstr(my_mp3_file, ".JPG")!=NULL || strstr(my_mp3_file, ".jpeg")!=NULL || strstr(my_mp3_file, ".JPEG")!=NULL)
				{
						//cellDbgFontDrawGcm(); cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

						if(pic_reload!=0){
							cellDbgFontDrawGcm();
							pic_zoom=-1.0f;
							if(strstr(list[current_image].path,"/net_host")!=NULL) //network copy
							{

								char cpath[1024], cpath2[1024];
								int chost=0; int pl=strlen(list[current_image].path);
								chost=list[current_image].path[9]-0x30;
								for(int n=11;n<pl;n++)
								{cpath[n-11]=list[current_image].path[n]; cpath[n-10]=0;}
								sprintf(cpath2, "/%s", cpath); //host_list[chost].root,, 
								sprintf(my_mp3_file, "%s/TEMP/net_view.bin", app_usrdir);
								network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) my_mp3_file, 0);
							}

							if(strstr(list[current_image].path,"/pvd_usb")!=NULL) //ntfs
							{
								sprintf(my_mp3_file, "%s/TEMP/net_view.bin", app_usrdir);
								file_copy(list[current_image].path, my_mp3_file, 0);
							}

							load_jpg_texture(text_FONT, my_mp3_file, 1920);
							slide_time=0;
						}
						png_w2=png_w; png_h2=png_h;
						if(pic_zoom==-1.0f){
							pic_zoom=1.0f;
							//if(png_w==1280 && png_h==720) pic_zoom=1920.0f/1280.0f;
							//if(png_w==640 && png_h==360) pic_zoom=3.0f;
							if(png_h!=0 && png_h>=png_w && (float)png_h/(float)png_w>=1.77f)
								pic_zoom=float (1080.0f / png_h);
							if(png_h!=0 && png_h>=png_w && (float)png_h/(float)png_w<1.77f)
								pic_zoom=float (1920.0f / png_h);
							else if(png_w!=0 && png_h!=0 && png_w>png_h && (float)png_w/(float)png_h>=1.77f)
								pic_zoom=float (1920.0f / png_w);
							else if(png_w!=0 && png_h!=0 && png_w>png_h && (float)png_w/(float)png_h<1.77f)
								pic_zoom=float (1080.0f / png_h);

						}
						if(pic_zoom>4.f) pic_zoom=4.f;
						png_h2=(int) (png_h2*pic_zoom);
						png_w2=(int) (png_w2*pic_zoom);
						if(pic_reload!=0)
						{
							if(slide_dir==0)
								for(int slide_in=1920; slide_in>=0; slide_in-=128)
								{	flip();
									cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
									set_texture( text_FONT, 1920, 1080); 
									display_img((int)((1920-png_w2)/2)+pic_posX+slide_in, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
								}
							else
								for(int slide_in=-1920; slide_in<=0; slide_in+=128)
								{	flip();
									cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
									set_texture( text_FONT, 1920, 1080); 
									display_img((int)((1920-png_w2)/2)+pic_posX+slide_in, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
								}
						}
						else
							{
								cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
								set_texture( text_FONT, 1920, 1080); 
								display_img((int)((1920-png_w2)/2)+pic_posX, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
							}
/*						if(show_info==1){

							sprintf(pic_info,"Name: %s", list[current_image].name); pic_info[95]=0;
							draw_text_stroke( 0.04f+new_offset, 0.867f, 0.7f ,0xc0a0a0a0, pic_info);
							timeinfo = localtime ( &list[current_image].time );
							sprintf(pic_info,"Info: JPEG %ix%i (Zoom: %3.0f)\nDate: %02d/%02d/%04d", png_w, png_h, pic_zoom*100.0f, timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
							draw_text_stroke( 0.04f+new_offset, 0.89f, 0.7f ,0xc0a0a0a0, pic_info);

							cellDbgFontDrawGcm();
						}
*/

				}

				if(strstr(my_mp3_file, ".png")!=NULL || strstr(my_mp3_file, ".PNG")!=NULL)
				{
						cellDbgFontDrawGcm();
						if(pic_reload!=0){
							if(strstr(list[current_image].path,"/net_host")!=NULL) //network copy
							{

								char cpath[1024], cpath2[1024];
								int chost=0; int pl=strlen(list[current_image].path);
								chost=list[current_image].path[9]-0x30;
								for(int n=11;n<pl;n++)
								{cpath[n-11]=list[current_image].path[n]; cpath[n-10]=0;}
								sprintf(cpath2, "/%s", cpath); //host_list[chost].root,
								sprintf(my_mp3_file, "%s/TEMP/net_view.bin", app_usrdir);
								network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) my_mp3_file, 0);
							}

							if(strstr(list[current_image].path,"/pvd_usb")!=NULL) //ntfs
							{
								sprintf(my_mp3_file, "%s/TEMP/net_view.bin", app_usrdir);
								file_copy(list[current_image].path, my_mp3_file, 0);
							}

							load_png_texture(text_FONT, my_mp3_file, 1920);
							slide_time=0;
							//if(png_w>1920 || png_h>1080) goto cancel_exit;
						}

						png_w2=png_w; png_h2=png_h;
						if(pic_zoom==-1.0f){
							pic_zoom=1.0f;
							//if(png_w==1280 && png_h==720) pic_zoom=1920.0f/1280.0f;
							//if(png_w==640 && png_h==360) pic_zoom=3.0f;
							if(png_h!=0 && png_h>=png_w && (float)png_h/(float)png_w>=1.77f)
								pic_zoom=float (1080.0f / png_h);
							if(png_h!=0 && png_h>=png_w && (float)png_h/(float)png_w<1.77f)
								pic_zoom=float (1920.0f / png_h);
							else if(png_w!=0 && png_h!=0 && png_w>png_h && (float)png_w/(float)png_h>=1.77f)
								pic_zoom=float (1920.0f / png_w);
							else if(png_w!=0 && png_h!=0 && png_w>png_h && (float)png_w/(float)png_h<1.77f)
								pic_zoom=float (1080.0f / png_h);
						}
						if(pic_zoom>4.f) pic_zoom=4.f;
						png_h2=(int) (png_h2*pic_zoom);
						png_w2=(int) (png_w2*pic_zoom);
						pic_X=(int)((1920-png_w2)/2)+pic_posX;
						pic_Y=(int)((1080-png_h2)/2)+pic_posY;

						if(pic_reload!=0)
						{
							if(slide_dir==0)
								for(int slide_in=1920; slide_in>=0; slide_in-=128)
								{	flip();
									cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
									set_texture( text_FONT, 1920, 1080); 
									display_img((int)((1920-png_w2)/2)+pic_posX+slide_in, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
								}
							else
								for(int slide_in=-1920; slide_in<=0; slide_in+=128)
								{	flip();
									cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
									set_texture( text_FONT, 1920, 1080); 
									display_img((int)((1920-png_w2)/2)+pic_posX+slide_in, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
								}
						}
						else
							{
								cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
								set_texture( text_FONT, 1920, 1080); 
								display_img(pic_X, pic_Y, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
							}

				}

					//flip();
					int ci=current_image;
					to_break=0;
					char ss_status[8];

					while(1){
						pad_read();
						cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
						set_texture( text_FONT, 1920, 1080); 
						if(strstr(my_mp3_file, ".png")!=NULL || strstr(my_mp3_file, ".PNG")!=NULL)
							display_img(pic_X, pic_Y, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
						else
							display_img((int)((1920-png_w2)/2)+pic_posX, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
						if(show_info==1){
							if(slide_show) sprintf(ss_status, "%s", "Stop"); else sprintf(ss_status, "%s", "Start");
							sprintf(pic_info,"   Name: %s", list[current_image].name); pic_info[95]=0;
							draw_text_stroke( 0.04f+new_offset, 0.867f, 0.7f ,0xc0a0a0a0, pic_info);
							timeinfo = localtime ( &list[current_image].time );
							if(strstr(my_mp3_file, ".png")!=NULL || strstr(my_mp3_file, ".PNG")!=NULL)
								sprintf(pic_info,"   Info: PNG %ix%i (Zoom: %3.0f)\n   Date: %02d/%02d/%04d\n[START]: %s slideshow", png_w, png_h, pic_zoom*100.0f, timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, ss_status);
							else
								sprintf(pic_info,"   Info: JPEG %ix%i (Zoom: %3.0f)\n   Date: %02d/%02d/%04d\n[START]: %s slideshow", png_w, png_h, pic_zoom*100.0f, timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, ss_status);

							draw_text_stroke( 0.04f+new_offset, 0.89f, 0.7f ,0xc0a0a0a0, pic_info);

							cellDbgFontDrawGcm();
						}
						flip();

						if ( new_pad & BUTTON_SELECT ) {show_info=1-show_info; pic_reload=0; break;}// new_pad=0; old_pad=0;

						if ( new_pad & BUTTON_START ) {
							slide_time=0; //new_pad=0; old_pad=0;
							slide_show=1-slide_show; slide_dir=0;
						}

						if(slide_show==1) slide_time++;

						if ( ( new_pad & BUTTON_TRIANGLE ) || ( new_pad & BUTTON_CIRCLE ) ) {new_pad=0; old_pad=0;to_break=1;break;}

						if ( ( new_pad & BUTTON_RIGHT ) || ( new_pad & BUTTON_R1 ) || ( new_pad & BUTTON_CROSS ) || (slide_show==1 && slide_time>600) )
						{
							//find next image in the list
							int one_time3=1;
check_from_start3:
							for(ci=current_image+1; ci<pane_size; ci++)
							{
								sprintf(my_mp3_file, "%s", list[ci].path);
								if(strstr(my_mp3_file, ".jpg")!=NULL || strstr(my_mp3_file, ".JPG")!=NULL || strstr(my_mp3_file, ".jpeg")!=NULL || strstr(my_mp3_file, ".JPEG")!=NULL || strstr(my_mp3_file, ".png")!=NULL || strstr(my_mp3_file, ".PNG")!=NULL)
								{
									current_image=ci; 
									pic_zoom=1.0f;
									pic_reload=1;
									pic_posX=pic_posY=0;
									slide_time=0;
									slide_dir=0;
									break;
								}

							}
							
							if((current_image>pane_size || ci>=pane_size) && one_time3) {one_time3=0; current_image=-1; goto check_from_start3;}//to_break=1; // || current_image==e
							break;
							
						}

						if ( ( new_pad & BUTTON_LEFT ) || ( new_pad & BUTTON_L1 ) ) 
						{
							//find previous image in the list
							if(current_image==0) current_image=pane_size;
							int one_time=1;
check_from_start:
							for(ci=current_image-1; ci>=0; ci--)
							{
								sprintf(my_mp3_file, "%s", list[ci].path);
								if(strstr(my_mp3_file, ".jpg")!=NULL || strstr(my_mp3_file, ".JPG")!=NULL || strstr(my_mp3_file, ".jpeg")!=NULL || strstr(my_mp3_file, ".JPEG")!=NULL || strstr(my_mp3_file, ".png")!=NULL || strstr(my_mp3_file, ".PNG")!=NULL)
								{
									current_image=ci; 
									pic_zoom=1.0f;
									pic_reload=1;
									pic_posX=pic_posY=0;
									slide_show=0; slide_dir=1;
									break;
								}

							}
							
							if((current_image<0 || ci<0) && one_time) {one_time=0; current_image=pane_size; goto check_from_start;}// to_break=1; // || current_image==e
							break;
							
						}

						if (( new_pad & BUTTON_L3 ) || ( new_pad & BUTTON_DOWN )) 
						{
							if(png_w!=0 && pic_zoom==1.0f)
								pic_zoom=float (1920.0f / png_w);
							else
								pic_zoom=1.0f;
							pic_reload=0;
							pic_posX=pic_posY=0;
							new_pad=0; old_pad=0;
							break;
						}

						if (( new_pad & BUTTON_R3 ) || ( new_pad & BUTTON_UP )) 
						{
							if(png_h!=0 && pic_zoom==1.0f)
								pic_zoom=float (1080.0f / png_h);
							else
								pic_zoom=1.0f;
							pic_reload=0;
							pic_posX=pic_posY=0;
							new_pad=0; old_pad=0;
							break;
						}

						if (mouseXDL!=0.0f && png_w2>1920) 
						{
							pic_posX-=(int) (mouseXDL*1920.0f);
							pic_reload=0;

							if( pic_posX<(int)((1920-png_w2)/2) ) pic_posX=(int)((1920-png_w2)/2);
							if( ((int)((1920-png_w2)/2)+pic_posX)>0 ) pic_posX=0-(int)((1920-png_w2)/2);
							break;
						}

						if (mouseYDL!=0.0f && png_h2>1080) 
						{
							pic_posY-=(int) (mouseYDL*1080.0f);

							if( pic_posY<(int)((1080-png_h2)/2) ) pic_posY=(int)((1080-png_h2)/2);
							if( ((int)((1080-png_h2)/2)+pic_posY)>0 ) pic_posY=0-(int)((1080-png_h2)/2);

							pic_reload=0;
							break;
						}

						if (( new_pad & BUTTON_L2 ) || mouseXDR> 0.003f || mouseYDR> 0.003f) 
						{
							if ( new_pad & BUTTON_L2 )
								pic_zoom-=0.045f;
							else
								pic_zoom-=0.010f;
							if(pic_zoom<1.0f) pic_zoom=1.000f;
							pic_reload=0;
							png_h2=(int) (png_h2*pic_zoom);
							png_w2=(int) (png_w2*pic_zoom);
							if( pic_posX<(int)((1920-png_w2)/2) ) pic_posX=(int)((1920-png_w2)/2);
							if( ((int)((1920-png_w2)/2)+pic_posX)>0 ) pic_posX=0;
							if( pic_posY<(int)((1080-png_h2)/2) ) pic_posY=(int)((1080-png_h2)/2);
							if( ((int)((1080-png_h2)/2)+pic_posY)>0 ) pic_posY=0;
							break;
						}

						if (( new_pad & BUTTON_R2 ) || mouseXDR< -0.003f || mouseYDR< -0.003f) 
						{
							if (new_pad & BUTTON_R2) 
								pic_zoom+=0.045f;
							else
								pic_zoom+=0.010f;
							pic_reload=0;
							png_h2=(int) (png_h2*pic_zoom);
							png_w2=(int) (png_w2*pic_zoom);
							if( pic_posX<(int)((1920-png_w2)/2) ) pic_posX=(int)((1920-png_w2)/2);
							if( ((int)((1920-png_w2)/2)+pic_posX)>0 ) pic_posX=0;
							if( pic_posY<(int)((1080-png_h2)/2) ) pic_posY=(int)((1080-png_h2)/2);
							if( ((int)((1080-png_h2)/2)+pic_posY)>0 ) pic_posY=0;
							break;
						}

					}
					new_pad=0;//old_pad=0;

					if(to_break==1) break; 

				} //picture viewer
				use_analog=0;
//				load_texture(text_MSC_5, iconMSC, 320);
				}
			state_draw=1;	
			}

cancel_exit:
	
//			if ((new_pad & BUTTON_SQUARE) && (list[e].type==1) && strstr(this_pane,"/ps3_home")==NULL) 
			if (!strcmp(fm_func, "delete") && (list[e].type==1)) 
			{
//fm delete file
				int m_copy;
				int m_copy_total=0;
				for(m_copy=0; m_copy<pane_size; m_copy++) if(list[m_copy].type==1) m_copy_total+=list[m_copy].selected;
				if (m_copy_total==0) {list[e].selected=1; m_copy_total=1;}

				new_pad=0; old_pad=0;
				if(m_copy_total==1)
					sprintf(filename, "Do you want to delete the selected file?\n\n[%s]", list[e].path);
				else
					sprintf(filename, "Do you want to delete the selected %i files?", m_copy_total);

				dialog_ret=0;
				cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				wait_dialog();
				
				if(dialog_ret==1)
				{
					for(m_copy=0; m_copy<pane_size; m_copy++) 
					if(list[m_copy].selected && list[m_copy].type==1)
					{
						if(strstr(this_pane, "/net_host")!=NULL){

							char cpath2[1024];
							int chost=0;
							chost=list[m_copy].path[9]-0x30;
							if(strlen(list[m_copy].path)>11) {
								sprintf(cpath2, "/%s", list[e].path+11);
								network_del((char*)"DEL", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) "blank", 3);
								network_com((char*)"GET!",(char*)host_list[chost].host, host_list[chost].port, (char*)"/", (char*) host_list[chost].name, 1);
							}
						}
						else
							remove(list[m_copy].path);
					}
					if(x_offset>=0.54f) state_read=3; else state_read=2;
					if(!strcmp(this_pane, other_pane)) state_read=1;
				}
			}

//			if ((new_pad & BUTTON_SQUARE) && (list[e].type==0) && (list[e].name[0]!=0x2e) && strlen(list[e].path)>12 && strstr(this_pane, "/ps3_home")==NULL && disable_options!=1 && disable_options!=3) 
			if (!strcmp(fm_func, "delete") && (list[e].type==0) && strstr(list[e].path, "net_host")==NULL) 
			{
//fm delete folder
				sprintf(fm_func, "%s", "none");
				new_pad=0; old_pad=0;
				int m_copy;
				int m_copy_total=0;
				for(m_copy=0; m_copy<pane_size; m_copy++) if(list[m_copy].type==0) m_copy_total+=list[m_copy].selected;
				if (m_copy_total==0) {list[e].selected=1; m_copy_total=1;}

				if(m_copy_total==1)
					sprintf(filename, "Do you want to delete the selected folder and its contents?\n\n[%s]", list[e].path);
				else
					sprintf(filename, "Do you want to delete the selected %i folders and their contents?", m_copy_total);
				dialog_ret=0;
				cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				wait_dialog();
				
				if(dialog_ret==1)
				{
					for(m_copy=0; m_copy<pane_size; m_copy++) 
					if(list[m_copy].selected && list[m_copy].type==0)
					{

							if(strstr(this_pane, "/net_host")!=NULL){

								char cpath2[1024];
								int chost=0;
								chost=list[m_copy].path[9]-0x30;
								if(strlen(list[m_copy].path)>11) {
									sprintf(cpath2, "/%s/", list[m_copy].path+11);
									network_del((char*)"DEL", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) "blank", 3);
									network_com((char*)"GET!",(char*)host_list[chost].host, host_list[chost].port, (char*)"/", (char*) host_list[chost].name, 1);
								}
							}
						else

						{


						abort_copy=0;
						time_start= time(NULL);
						file_counter=0;
						my_game_delete((char *) list[m_copy].path);
						rmdir((char *) list[m_copy].path);
						if(strstr(list[m_copy].path, "/dev_hdd")!=NULL) forcedevices|=0x0001;
						if(strstr(list[m_copy].path, "/dev_usb")!=NULL) forcedevices|=0xFFFE;
						}
						if(x_offset>=0.54f) state_read=3; else state_read=2;
					}
				}
			}

//			if ( ((new_pad & BUTTON_CIRCLE) || (new_pad & BUTTON_R3)) && (list[e].type==0) && (list[e].name[0]!=0x2e) && ((strlen(other_pane)>5 && strcmp(this_pane, other_pane)!=0) || ((new_pad & BUTTON_R3) && strstr(list[e].name, "PS3_GAME")!=NULL) ) && (!(strstr(other_pane, "/net_host")!=NULL && strstr(this_pane, "/net_host")!=NULL))  && (strstr(other_pane, "/ps3_home")==NULL || strstr(other_pane, "/ps3_home/video")!=NULL || strstr(other_pane, "/ps3_home/music")!=NULL || strstr(other_pane, "/ps3_home/photo")!=NULL)) 

			if (!strcmp(fm_func, "shortcut") && (list[e].type==1)) 
			{
				sprintf(fm_func, "%s", "none");
//fm shortcut file
				sprintf(filename, "%s/%s", other_pane, list[e].name);
				sprintf(string1, "%s", list[e].path);
				unlink(filename);
				link(string1, filename);
				state_read=1;
				state_draw=1;
			}

			if ( (!strcmp(fm_func, "copy") || !strcmp(fm_func, "move") || !strcmp(fm_func, "bdmirror") || !strcmp(fm_func, "pkgshortcut") || !strcmp(fm_func, "shortcut")) && (list[e].type==0)) 
			{
//fm copy folder

				use_symlinks=0;
				do_move=0; int ret, net_copy=0;
//				if((old_pad & BUTTON_SELECT)) do_move=1;
//				if(new_pad & BUTTON_R3) { use_symlinks=1; do_move=0; }

				if(!strcmp(fm_func, "move")) do_move=1;
				if(!strcmp(fm_func, "shortcut") || !strcmp(fm_func, "pkgshortcut") || !strcmp(fm_func, "bdmirror")) { use_symlinks=1; do_move=0; }

				if(strstr(list[e].path, "/net_host")!=NULL) {do_move=0; net_copy=1;}
				if(strstr(this_pane, "/ps3_home")!=NULL) do_move=0;

				int m_copy;
				int m_copy_total=0;
				for(m_copy=0; m_copy<pane_size; m_copy++) if(list[m_copy].type==0) m_copy_total+=list[m_copy].selected;
				if (m_copy_total==0) {list[e].selected=1; m_copy_total=1;}

				new_pad=0; old_pad=0;

				if(do_move==1)
					sprintf(filename, "Do you want to move the selected folders?\n\nSource: [%s]\n\nDestination: [%s]", list[e].path, other_pane);
				else
				{
					if(use_symlinks==1)
					{
						if(strstr(list[e].name, "PS3_GAME")!=NULL && strstr(list[e].path, "/dev_hdd0")!=NULL && !strcmp(fm_func, "pkgshortcut"))
							sprintf(filename, "Do you want to create a shadow copy of the selected folder?\n\nSource: [%s]\n\nDestination: [/dev_hdd0/G/<special_pkg_id>", list[e].path);
						else
						if(strstr(list[e].name, "PS3_GAME")!=NULL && strstr(list[e].path, "/dev_usb")!=NULL && (c_firmware==3.41f || c_firmware==3.55f || c_firmware==3.15f) && !strcmp(fm_func, "bdmirror"))
							sprintf(filename, "Do you want to enable BD-ROM GAME DISC mirror on external USB?\n\nSource: [%s]\n\nDestination: [Emulated BD-ROM on USB device]", list[e].path);
						else
							sprintf(filename, "Do you want to create a shadow copy of the selected folder?\n\nSource: [%s]\n\nDestination: [%s/%s]", list[e].path, other_pane, list[e].name);

					}
					else
						sprintf(filename, "Do you want to copy the selected folders?\n\nSource: [%s]\n\nDestination: [%s]", list[e].path, other_pane);
				}

				dialog_ret=0;
				cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				wait_dialog();

//				if(strstr(list[e].name, "PS3_GAME")!=NULL && strstr(list[e].path, "/dev_usb")!=NULL && dialog_ret==1 && use_symlinks==1)
				if(!strcmp(fm_func, "bdmirror") && dialog_ret==1 && use_symlinks==1)
				{
					sprintf(fm_func, "%s", "none");

					if(c_firmware!=3.55f && c_firmware!=3.41f &&  c_firmware!=3.15f)
					{
						dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, "Your current configuration doesn't support this function!", dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
						goto cancel_mount;
					}

//					if(stat((char*)"/dev_bdvd/PS3_GAME/PARAM.SFO", &s3)<0)
//					{
//						dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, "To use this feature you have to insert any original Playstation(R)3 game disc!", dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
//						goto cancel_mount;
//					}

					char just_drive[32];
					char usb_mount0[512], usb_mount1[512], usb_mount2[512];
					char path_backup[512], path_bup[512];
					FILE *fpA;
					strncpy(just_drive, list[e].path, 11); just_drive[11]=0;
					ret = mod_mount_table(just_drive, 0); //restore

					if(ret)
					{
						sprintf(usb_mount1, "%s/PS3_GAME", just_drive);

						if(stat(usb_mount1, &s3)>=0) 
						{
							//restore PS3_GAME back to USB game folder
							sprintf(path_bup, "%s/PS3PATH.BUP", usb_mount1);
							if(stat(path_bup, &s3)>=0) {
								fpA = fopen ( path_bup, "r" );
								if(fgets ( usb_mount2, 512, fpA )==NULL) sprintf(usb_mount2, "%s/PS3_GAME_OLD", just_drive);
								fclose(fpA);
								strncpy(usb_mount2, just_drive, 11); //always use current device

							}
							else
								sprintf(usb_mount2, "%s/PS3_GAME_OLD", just_drive);

								int pl, n; char tempname[512];
								pl=strlen(usb_mount2);
								for(n=0;n<pl;n++)
								{
									tempname[n]=usb_mount2[n];
									tempname[n+1]=0;
									if(usb_mount2[n]==0x2F && stat(tempname, &s3)<0) 
									{	
										mkdir(tempname, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(tempname, 0777);
									}
								}


							if(stat(usb_mount2, &s3)<0) rename (usb_mount1, usb_mount2);

						}


						if(stat(usb_mount1, &s3)<0) 
						{

							sprintf(usb_mount0, "%s", list[e].path);
							sprintf(path_backup, "%s/PS3PATH.BUP", usb_mount0);
							remove(path_backup);
							fpA = fopen ( path_backup, "w" );
							fputs ( list[e].path,  fpA );
							fclose(fpA);
							rename (usb_mount0, usb_mount1);
							if(stat((char*)"/dev_bdvd/PS3_GAME/PARAM.SFO", &s3)<0)
								sprintf(string1,"multiMAN will now exit to XMB\xE2\x84\xA2 and you can start the game from the /app_home icon.");
							else
								sprintf(string1,"multiMAN will now exit to XMB\xE2\x84\xA2 and you can start the game from the Blu-ray\xE2\x84\xA2 Game Disc icon.");
							ret = mod_mount_table(just_drive, 1); //modify
							if(ret)
							{
								dialog_ret=0;
								cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL );
								wait_dialog();
								unload_modules(); sys_process_exit(1);

							}
							else
							{
								ret = mod_mount_table((char *) "reset", 0); //reset
								rename (usb_mount1, usb_mount0);
								dialog_ret=0; ret = cellMsgDialogOpen2( type_dialog_ok, "Error occured while parsing device mount table!", dialog_fun2, (void*)0x0000aaab, NULL );; wait_dialog();
								goto cancel_mount;
							}

						}
						else
						{
							ret = mod_mount_table((char *) "reset", 0); //reset
							dialog_ret=0;
							ret = cellMsgDialogOpen2( type_dialog_ok, "Error occured while moving game to new location!", dialog_fun2, (void*)0x0000aaab, NULL );					
							wait_dialog();
						}


					}
					else
					{
						dialog_ret=0;
						ret = mod_mount_table((char *) "reset", 0); //reset
						ret = cellMsgDialogOpen2( type_dialog_ok, "Error occured while parsing device mount table!", dialog_fun2, (void*)0x0000aaab, NULL );					
						wait_dialog();
					}

cancel_mount:

					dialog_ret=0;
				}
				
				if(dialog_ret==1)
				{

				char m_other_pane[512];
				sprintf(m_other_pane, "%s", other_pane);

				for(m_copy=0; m_copy<pane_size; m_copy++) 
				if(list[m_copy].selected && list[m_copy].type==0)
				{
					time_start= time(NULL);
	
					abort_copy=0;
					initConsole();
					file_counter=0;
					new_pad=0;

					if(strstr(other_pane,"/dev_usb")!=NULL || strstr(other_pane,"/dev_sd")!=NULL || strstr(other_pane,"/dev_ms")!=NULL || strstr(other_pane,"/dev_cf")!=NULL) copy_mode=1; // break files >= 4GB
						else copy_mode=0;

					copy_is_split=0;
					sprintf(temp_pane, "%s", other_pane);
					sprintf(other_pane,"%s/%s", m_other_pane, list[m_copy].name);

					if(strstr(list[m_copy].name, "PS3_GAME")!=NULL && !strcmp(fm_func, "pkgshortcut") && use_symlinks==1)
					{
						sprintf(fm_func, "%s", "none");
						sprintf(filename, "%s/PARAM.SFO", list[m_copy].path);
						char just_title[128], just_title_id[64]; just_title[0]=0; just_title_id[0]=0; int plevel;
						parse_param_sfo(filename, just_title, just_title_id,  &plevel);
//						sprintf(other_pane,"%s/%s", temp_pane, just_title_id);

						sprintf(other_pane,"/dev_hdd0/G");
						mkdir(other_pane, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(other_pane, 0777);

						char just_title_id2[7];
						//just_title_id[0]=0x5A;//Z
						just_title_id2[0]=just_title_id[2];
						if(just_title_id[1]!=0x4C) just_title_id2[0] = (just_title_id[2] | 0x20);
						just_title_id2[1]=just_title_id[4];	just_title_id2[2]=just_title_id[5];
						just_title_id2[3]=just_title_id[6];	just_title_id2[4]=just_title_id[7];
						just_title_id2[5]=just_title_id[8]; just_title_id2[6]=0;

						sprintf(other_pane,"/dev_hdd0/game/%s", just_title_id);

/*						if(stat(other_pane, &s3)>=0)
						{
							dialog_ret=0;
							sprintf(filename, "Destination already contains folder with the same name!\n\nContinue and overwrite?\n\n[%s]", other_pane );
							cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
							wait_dialog();
	
							if(dialog_ret!=1) goto overwrite_cancel_3;
						}
*/

						mkdir(other_pane, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(other_pane, 0777);
						my_game_copy((char *) list[m_copy].path, (char *) other_pane); 
						cellMsgDialogAbort();

						sprintf(other_pane,"/dev_hdd0/G/%s", just_title_id2);
						mkdir(other_pane, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(other_pane, 0777);
						my_game_copy((char *) list[m_copy].path, (char *) other_pane); 

						sprintf(filename,   "/dev_hdd0/game/%s", just_title_id);
						sprintf(other_pane, "/dev_hdd0/G/%s", just_title_id2);
						my_game_copy((char *) filename, (char *) other_pane); 

						sprintf(filename,   "/dev_hdd0/game/%s/USRDIR/MM_NON_NPDRM_EBOOT.BIN", just_title_id);
						sprintf(other_pane, "/dev_hdd0/G/%s/USRDIR/EBOOT.BIN", just_title_id2);
						if(stat(filename, &s3)>=0) {
							unlink(other_pane);
							remove(other_pane);
							rename(filename, other_pane);
							//file_copy(filename, other_pane, 0);
						}

//						sprintf(filename, "%s/PARAM.SFO", list[m_copy].path);
//						sprintf(string1 ,"/dev_hdd0/game/%s/PARAM.SFO", just_title_id);
//						file_copy(filename, string1, 0);
//						sprintf(filename, "%s/PARAM.SFO", list[m_copy].path);
//						sprintf(string1,"/dev_hdd0/G/%s/PARAM.SFO", just_title_id2);
//						file_copy(filename, string1, 0);

						cellMsgDialogAbort();
						sprintf(fm_func, "%s", "none");
						goto all_prompts_done;
					}


					if(strcmp(other_dev, this_dev)==0 && do_move==1 && !strcmp(fm_func, "move")) rename ( list[m_copy].path, other_pane );
					else
					{
						sprintf(fm_func, "%s", "none");
						if(strstr(other_pane,"/net_host")!=NULL){
							mkdir(other_pane, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(other_pane, 0777);
							net_folder_copy_put((char *) list[m_copy].path, (char *) temp_pane, (char *) list[m_copy].name); }

						else if(net_copy==1){
							mkdir(other_pane, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(other_pane, 0777);
							net_folder_copy((char *) list[m_copy].path, (char *) temp_pane, (char *) list[m_copy].name); }
						else
						{
							char tmp_path[512];
							sprintf(tmp_path, "%s", list[m_copy].path);
							if(strstr("/ps3_home", tmp_path)!=NULL) {mkdir(other_pane, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(other_pane, 0777);}
							if(strstr("/ps3_home/music", tmp_path)!=NULL) sprintf(tmp_path, "/dev_hdd0/music");
							if(strstr("/ps3_home/video", tmp_path)!=NULL) sprintf(tmp_path, "/dev_hdd0/video");
							if(strstr("/ps3_home/photo", tmp_path)!=NULL) sprintf(tmp_path, "/dev_hdd0/photo");
							if(strstr(other_pane, "/ps3_home/video")!=NULL)
								copy_nr( (char *)tmp_path, (char *) other_pane, (char *) list[m_copy].name); // recursive to single folder copy
							else if(strstr(other_pane, "/ps3_home/music")!=NULL)
								copy_nr( (char *)tmp_path, (char *) other_pane, (char *) list[m_copy].name);
							else if(strstr(other_pane, "/ps3_home/photo")!=NULL)
								copy_nr( (char *)tmp_path, (char *) other_pane, (char *) list[m_copy].name);
							else
								{
									dialog_ret=1;
									if(stat(other_pane, &s3)==0)
									{
										dialog_ret=0;
										sprintf(filename, "Destination already contains folder with the same name!\n\nContinue and overwrite?\n\n[%s]", other_pane );
										cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
										wait_dialog();
									}	
									if(dialog_ret!=1) goto overwrite_cancel_3;

									mkdir(other_pane, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(other_pane, 0777);
									my_game_copy((char *) tmp_path, (char *) other_pane); 


								}
						}

						ret=cellMsgDialogAbort();

						if(do_move==1 && abort_copy==0) { my_game_delete((char *) list[m_copy].path); rmdir((char *) list[m_copy].path); }
					}
all_prompts_done:
					if(x_offset>=0.54f) state_read=2; else state_read=3;
					if(do_move==1) state_read=1;
					termConsole();
					cellMsgDialogAbort();
					if( (do_move==1 && strstr(list[m_copy].path, "/dev_hdd")!=NULL) || (strstr(other_pane, "/dev_hdd")!=NULL)) forcedevices|=0x0001;
					if( (do_move==1 && strstr(list[m_copy].path, "/dev_usb")!=NULL) || (strstr(other_pane, "/dev_usb")!=NULL)) forcedevices|=0xFFFE;
					if(abort_copy!=0) break;
				}
				sprintf(fm_func, "%s", "none");
				}
				sprintf(fm_func, "%s", "none");
			}

overwrite_cancel_3:

//			if ((new_pad & BUTTON_CIRCLE) && (list[e].type==1) && (list[e].name[0]!=0x2e) && strlen(other_pane)>5 && strcmp(this_pane, other_pane)!=0 && (!(strstr(other_pane, "/net_host")!=NULL && strstr(this_pane,"/net_host")!=NULL))  && (strstr(other_pane, "/ps3_home")==NULL || strstr(other_pane, "/ps3_home/video")!=NULL || strstr(other_pane, "/ps3_home/music")!=NULL || strstr(other_pane, "/ps3_home/photo")!=NULL) && disable_options!=2 && disable_options!=3) 
			if ((!strcmp(fm_func, "copy") || !strcmp(fm_func, "move")) && (list[e].type==1) && (!(strstr(other_pane, "/net_host")!=NULL && strstr(this_pane,"/net_host")!=NULL))  && (strstr(other_pane, "/ps3_home")==NULL || strstr(other_pane, "/ps3_home/video")!=NULL || strstr(other_pane, "/ps3_home/music")!=NULL || strstr(other_pane, "/ps3_home/photo")!=NULL)) 

			{
//fm copy file
				do_move=0;
//				if((old_pad & BUTTON_SELECT)) do_move=1;
				if(!strcmp(fm_func, "move")) do_move=1;
				sprintf(fm_func, "%s", "none");

				if(strstr(list[e].path, "/net_host")!=NULL) do_move=0;
				if(strstr(this_pane, "/ps3_home")!=NULL) do_move=0;
					int m_copy;
					int m_copy_total=0;
					for(m_copy=0; m_copy<pane_size; m_copy++) if(list[m_copy].type==1) m_copy_total+=list[m_copy].selected;
					if (m_copy_total==0) {list[e].selected=1; m_copy_total=1;}

				new_pad=0; old_pad=0;
				if(m_copy_total==1)
				{
					if(do_move==1)
					sprintf(filename, "Do you want to move the selected file?\n\nSource: [%s]\n\nDestination: [%s/%s]", list[e].path, other_pane, list[e].name);
						else
					sprintf(filename, "Do you want to copy the selected file?\n\nSource: [%s]\n\nDestination: [%s/%s]", list[e].path, other_pane, list[e].name);
				}
				else
				{
					if(do_move==1)
					sprintf(filename, "Do you want to move selected %i files?\n\nSource: [%s]\n\nDestination: [%s]", m_copy_total, this_pane, other_pane);
						else
					sprintf(filename, "Do you want to copy selected %i files?\n\nSource: [%s]\n\nDestination: [%s]", m_copy_total, this_pane, other_pane);
				}

				dialog_ret=0;
				cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				wait_dialog();
				
				if(dialog_ret==1)
				{
					time_start= time(NULL);
	
					abort_copy=0;
					file_counter=0;
					new_pad=0;

					if(strstr(other_pane,"/dev_usb")!=NULL || strstr(other_pane,"/dev_sd")!=NULL || strstr(other_pane,"/dev_ms")!=NULL || strstr(other_pane,"/dev_cf")!=NULL) copy_mode=1; // break files >= 4GB
						else copy_mode=0;

					copy_is_split=0;
					char m_other_pane[512];
					sprintf(m_other_pane, "%s", other_pane);

					for(m_copy=0; m_copy<pane_size; m_copy++) 
					if(list[m_copy].selected && list[m_copy].type==1)
					{
						list[m_copy].selected=0;
						sprintf(other_pane,"%s/%s", m_other_pane, list[m_copy].name); 

					if(strcmp(other_dev, this_dev)==0 && do_move==1) rename ( list[m_copy].path, other_pane );

					else
					{
						if(do_move==1)
							sprintf(string1, "Moving, please wait!");
						else
							sprintf(string1, "Copying, please wait!");

						cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
						cellDbgFontPrintf( 0.3f, 0.45f, 0.8f, 0xc0c0c0c0, string1);
						cellDbgFontDrawGcm();
						if(do_move==1)
							sprintf(string1, "Moving file, please wait...");
						else
							sprintf(string1, "Copying file, please wait...");


						if(strstr(other_pane, "/net_host")!=NULL){

							char cpath2[1024];
							int chost=0;
							chost=other_pane[9]-0x30;
							if(strlen(other_pane)>11)
								sprintf(cpath2, "/%s", other_pane+11);
							else
								sprintf(cpath2, "/%s", list[m_copy].name);
							char put_cmd[1024];
							sprintf(put_cmd, "PUT-%.f", (double) list[m_copy].size);

							network_put((char*)put_cmd, (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) list[m_copy].path, 3);
							network_com((char*)"GET!",(char*)host_list[chost].host, host_list[chost].port, (char*)"/", (char*) host_list[chost].name, 1);

//		network_put(char *command, char *server_name, int server_port, char *net_file, char *save_path, int show_progress )

						}

						else if(strstr(list[m_copy].path,"/net_host")!=NULL) //network copy
						{
							char cpath[1024], cpath2[1024];
							int chost=0; int pl=strlen(list[m_copy].path);
							chost=list[m_copy].path[9]-0x30;
							for(int n=11;n<pl;n++)
							{cpath[n-11]=list[m_copy].path[n]; cpath[n-10]=0;}
							sprintf(cpath2, "/%s", cpath); //host_list[chost].root,

							if(strstr(other_pane, "/ps3_home/video")!=NULL && (strstr(list[m_copy].path, ".avi")!=NULL || strstr(list[m_copy].path, ".AVI")!=NULL || strstr(list[m_copy].path, ".m2ts")!=NULL || strstr(list[m_copy].path, ".M2TS")!=NULL || strstr(list[m_copy].path, ".mts")!=NULL || strstr(list[m_copy].path, ".MTS")!=NULL || strstr(list[m_copy].path, ".m2t")!=NULL || strstr(list[m_copy].path, ".M2T")!=NULL || strstr(list[m_copy].path, ".divx")!=NULL || strstr(list[m_copy].path, ".DIVX")!=NULL || strstr(list[m_copy].path, ".mpg")!=NULL || strstr(list[m_copy].path, ".MPG")!=NULL || strstr(list[m_copy].path, ".mpeg")!=NULL || strstr(list[m_copy].path, ".MPEG")!=NULL || strstr(list[m_copy].path, ".mp4")!=NULL || strstr(list[m_copy].path, ".MP4")!=NULL || strstr(list[m_copy].path, ".vob")!=NULL || strstr(list[m_copy].path, ".VOB")!=NULL || strstr(list[m_copy].path, ".wmv")!=NULL || strstr(list[m_copy].path, ".WMV")!=NULL || strstr(list[m_copy].path, ".ts")!=NULL || strstr(list[m_copy].path, ".TS")!=NULL || strstr(list[m_copy].path, ".mov")!=NULL || strstr(list[m_copy].path, ".MOV")!=NULL) )
								{
									sprintf(other_pane,"%s/%s", app_temp, list[m_copy].name); 
									network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) other_pane, 3);

									if(stat(other_pane, &s3)>=0) video_export((char *) list[m_copy].name, (char*) "My video", 1);
								}
							else if(strstr(other_pane, "/ps3_home/music")!=NULL && (strstr(list[m_copy].path, ".mp3")!=NULL || strstr(list[m_copy].path, ".MP3")!=NULL || strstr(list[m_copy].path, ".wav")!=NULL || strstr(list[m_copy].path, ".WAV")!=NULL || strstr(list[m_copy].path, ".aac")!=NULL || strstr(list[m_copy].path, ".AAC")!=NULL) )
								{
									sprintf(other_pane,"%s/%s", app_temp, list[m_copy].name); 
									network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) other_pane, 3);

									if(stat(other_pane, &s3)>=0) music_export((char *) list[m_copy].name, (char*) "My music", 1);
								}
							else if(strstr(other_pane, "/ps3_home/photo")!=NULL && (strstr(list[m_copy].path, ".jpg")!=NULL || strstr(list[m_copy].path, ".JPG")!=NULL || strstr(list[m_copy].path, ".jpeg")!=NULL || strstr(list[m_copy].path, ".JPEG")!=NULL || strstr(list[m_copy].path, ".png")!=NULL || strstr(list[m_copy].path, ".PNG")!=NULL) )
								{
									sprintf(other_pane,"%s/%s", app_temp, list[m_copy].name); 
									network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) other_pane, 3);

									if(stat(other_pane, &s3)>=0) photo_export((char *) list[m_copy].name, (char*) "My photos", 1);
								}

							else if(strstr(other_pane, "/ps3_home")==NULL)
								network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) other_pane, 3);

						}
						else
						{
						cellMsgDialogOpen2(	CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL	|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE,string1,NULL,NULL,NULL);
						flip();

							if(strstr(other_pane, "/ps3_home/video")!=NULL && (strstr(list[m_copy].path, ".avi")!=NULL || strstr(list[m_copy].path, ".AVI")!=NULL || strstr(list[m_copy].path, ".m2ts")!=NULL || strstr(list[m_copy].path, ".M2TS")!=NULL || strstr(list[m_copy].path, ".mts")!=NULL || strstr(list[m_copy].path, ".MTS")!=NULL || strstr(list[m_copy].path, ".m2t")!=NULL || strstr(list[m_copy].path, ".M2T")!=NULL || strstr(list[m_copy].path, ".divx")!=NULL || strstr(list[m_copy].path, ".DIVX")!=NULL || strstr(list[m_copy].path, ".mpg")!=NULL || strstr(list[m_copy].path, ".MPG")!=NULL || strstr(list[m_copy].path, ".mpeg")!=NULL || strstr(list[m_copy].path, ".MPEG")!=NULL || strstr(list[m_copy].path, ".mp4")!=NULL || strstr(list[m_copy].path, ".MP4")!=NULL || strstr(list[m_copy].path, ".vob")!=NULL || strstr(list[m_copy].path, ".VOB")!=NULL || strstr(list[m_copy].path, ".wmv")!=NULL || strstr(list[m_copy].path, ".WMV")!=NULL || strstr(list[m_copy].path, ".ts")!=NULL || strstr(list[m_copy].path, ".TS")!=NULL || strstr(list[m_copy].path, ".mov")!=NULL || strstr(list[m_copy].path, ".MOV")!=NULL) )
							{
								sprintf(other_pane,"%s/%s", app_temp, list[m_copy].name); 
								file_copy((char *) list[m_copy].path, (char *) other_pane, 1);
								if(stat(other_pane, &s3)>=0) video_export((char *) list[m_copy].name, (char*) "My video", 1);
							}
						else if(strstr(other_pane, "/ps3_home/music")!=NULL && (strstr(list[m_copy].path, ".mp3")!=NULL || strstr(list[m_copy].path, ".MP3")!=NULL || strstr(list[m_copy].path, ".wav")!=NULL || strstr(list[m_copy].path, ".WAV")!=NULL || strstr(list[m_copy].path, ".aac")!=NULL || strstr(list[m_copy].path, ".AAC")!=NULL) )
							{
								sprintf(other_pane,"%s/%s", app_temp, list[m_copy].name); 
								file_copy((char *) list[m_copy].path, (char *) other_pane, 1);
								if(stat(other_pane, &s3)>=0) music_export((char *) list[m_copy].name, (char*) "My music", 1);
							}
						else if(strstr(other_pane, "/ps3_home/photo")!=NULL && (strstr(list[m_copy].path, ".jpg")!=NULL || strstr(list[m_copy].path, ".JPG")!=NULL || strstr(list[m_copy].path, ".jpeg")!=NULL || strstr(list[m_copy].path, ".JPEG")!=NULL || strstr(list[m_copy].path, ".png")!=NULL || strstr(list[m_copy].path, ".PNG")!=NULL) )
							{
								sprintf(other_pane,"%s/%s", app_temp, list[m_copy].name); 
								file_copy((char *) list[m_copy].path, (char *) other_pane, 1);
								if(stat(other_pane, &s3)>=0) photo_export((char *) list[m_copy].name, (char*) "My photos", 1);
							}
						else if(strstr(other_pane, "/ps3_home")==NULL)
							{
								file_copy((char *) list[m_copy].path, (char *) other_pane, 1);
								if(do_move==1 && abort_copy==0) remove(list[m_copy].path);
								cellMsgDialogAbort();
							}


						}
					}
					}
					if(x_offset>=0.54f) state_read=2; else state_read=3;
					if(do_move==1) state_read=1;
				}
				sprintf(fm_func, "%s", "none");
			}

			if( ((new_pad & BUTTON_SQUARE)) && (list[e].name[0]!=0x2e) && (!(strstr(other_pane, "/net_host")!=NULL && strstr(this_pane,"/net_host")!=NULL))  && (strstr(other_pane, "/ps3_home")==NULL || strstr(other_pane, "/ps3_home/video")!=NULL || strstr(other_pane, "/ps3_home/music")!=NULL || strstr(other_pane, "/ps3_home/photo")!=NULL))  //&& strlen(other_pane)>5 && strcmp(this_pane, other_pane)!=0
			{
				list[e].selected=1-list[e].selected;
				//new_pad=0;
			}


		}
//		if(c_opacity2>0x20)
		{
			if(x_offset>=0.54f)
			{
				print_label_width( x_offset,		  y_offset, 0.7f, color, str, 1.0f, 0.0f, 0, 0.225f);
//				print_label( x_offset+0.240f, y_offset, 0.7f, color, e_sizet, 1.0f, 0.0f, 0);
				print_label_ex( x_offset+0.295f, y_offset, 0.7f, color, e_sizet, 1.0f, 0.0f, 0, 1.0f, 1.0f, 2);
				print_label( x_offset+0.305f, y_offset, 0.7f, color, str_date, 1.0f, 0.0f, 0);

			}
			else
			{
//				print_label( x_offset+0.290f, y_offset, 0.7f, color, e_sizet, 1.0f, 0.0f, 0);
				print_label_width( x_offset,		  y_offset, 0.7f, color, str, 1.0f, 0.0f, 0, 0.27f);
				print_label_ex( x_offset+0.345f, y_offset, 0.7f, color, e_sizet, 1.0f, 0.0f, 0, 1.0f, 1.0f, 2);
				print_label( x_offset+0.355f, y_offset, 0.7f, color, str_date, 1.0f, 0.0f, 0);
			}

		}
		
//		cellDbgFontPrintf( x_offset, y_offset, 0.7f, color, str);
		y_offset+=0.026f;
	}
}


void parse_color_ini()
{
	FILE *fp;
	th_device_list=1;
	th_device_separator=1;
	th_device_separator_y=956;
	th_legend=1;
	th_legend_y=853;
	th_drive_icon=1;
	th_drive_icon_x=1790;
	th_drive_icon_y=964;

	if(stat(color_ini, &s3)>=0)
	{
		char col[16], line[128];
		int len=0, i=0;
		fp = fopen ( color_ini, "r" );

		while ( fgets ( line, sizeof line, fp ) != NULL ) /* read a line */
		{
			if(line[0]==35) continue;
	
			if(strstr (line,"PS3DISC=")!=NULL) { len = strlen(line)-2; 
				for(i = 8; i < len; i++) {col[i-8] = line[i];} col[i-8]=0;
				COL_PS3DISC=strtoul(col, NULL, 16);
				}

			if(strstr (line,"PS3DISCSEL=")!=NULL) { len = strlen(line)-2; 
				for(i = 11; i < len; i++) {col[i-11] = line[i];} col[i-11]=0;
				COL_PS3DISCSEL=strtoul(col, NULL, 16);
				}

			if(strstr (line,"SEL=")!=NULL) { len = strlen(line)-2; 
				for(i = 4; i < len; i++) {col[i-4] = line[i];} col[i-4]=0;
				COL_SEL=strtoul(col, NULL, 16);
				}
			if(strstr (line,"PS3=")!=NULL) { len = strlen(line)-2; 
				for(i = 4; i < len; i++) {col[i-4] = line[i];} col[i-4]=0;
				COL_PS3=strtoul(col, NULL, 16);
				}
			if(strstr (line,"PS2=")!=NULL) { len = strlen(line)-2; 
				for(i = 4; i < len; i++) {col[i-4] = line[i];} col[i-4]=0;
				COL_PS2=strtoul(col, NULL, 16);
				}
			if(strstr (line,"DVD=")!=NULL) { len = strlen(line)-2; 
				for(i = 4; i < len; i++) {col[i-4] = line[i];} col[i-4]=0;
				COL_DVD=strtoul(col, NULL, 16);
				}
			if(strstr (line,"BDMV=")!=NULL) { len = strlen(line)-2; 
				for(i = 5; i < len; i++) {col[i-5] = line[i];} col[i-5]=0;
				COL_BDMV=strtoul(col, NULL, 16);
				}
			if(strstr (line,"AVCHD=")!=NULL) { len = strlen(line)-2; 
				for(i = 6; i < len; i++) {col[i-6] = line[i];} col[i-6]=0;
				COL_AVCHD=strtoul(col, NULL, 16);
				}

			if(strstr (line,"LEGEND=")!=NULL) { len = strlen(line)-2; 
				for(i = 7; i < len; i++) {col[i-7] = line[i];} col[i-7]=0;
				COL_LEGEND=strtoul(col, NULL, 16);
				}

			if(strstr (line,"FMFILE=")!=NULL) { len = strlen(line)-2; 
				for(i = 7; i < len; i++) {col[i-7] = line[i];} col[i-7]=0;
				COL_FMFILE=strtoul(col, NULL, 16);
				}
			if(strstr (line,"FMDIR=")!=NULL) { len = strlen(line)-2; 
				for(i = 6; i < len; i++) {col[i-6] = line[i];} col[i-6]=0;
				COL_FMDIR=strtoul(col, NULL, 16);
				}
			if(strstr (line,"FMJPG=")!=NULL) { len = strlen(line)-2; 
				for(i = 6; i < len; i++) {col[i-6] = line[i];} col[i-6]=0;
				COL_FMJPG=strtoul(col, NULL, 16);
				}
			if(strstr (line,"FMMP3=")!=NULL) { len = strlen(line)-2; 
				for(i = 6; i < len; i++) {col[i-6] = line[i];} col[i-6]=0;
				COL_FMMP3=strtoul(col, NULL, 16);
				}
			if(strstr (line,"FMEXE=")!=NULL) { len = strlen(line)-2; 
				for(i = 6; i < len; i++) {col[i-6] = line[i];} col[i-6]=0;
				COL_FMEXE=strtoul(col, NULL, 16);
				}

			if(strstr (line,"HEXVIEW=")!=NULL) { len = strlen(line)-2; 
				for(i = 8; i < len; i++) {col[i-8] = line[i];} col[i-8]=0;
				COL_HEXVIEW=strtoul(col, NULL, 16);
				}

			if(strstr (line,"SPLIT=")!=NULL) { len = strlen(line)-2; 
				for(i = 6; i < len; i++) {col[i-6] = line[i];} col[i-6]=0;
				COL_SPLIT=strtoul(col, NULL, 16);
				}

			if(strstr (line,"XMB_CLOCK=")!=NULL) { len = strlen(line)-2; 
				for(i = 10; i < len; i++) {col[i-10] = line[i];} col[i-10]=0;
				COL_XMB_CLOCK=strtoul(col, NULL, 16);
				}

			if(strstr (line,"XMB_COLUMN=")!=NULL) { len = strlen(line)-2; 
				for(i = 11; i < len; i++) {col[i-11] = line[i];} col[i-11]=0;
				COL_XMB_COLUMN=strtoul(col, NULL, 16);
				}

			if(strstr (line,"XMB_TITLE=")!=NULL) { len = strlen(line)-2; 
				for(i = 10; i < len; i++) {col[i-10] = line[i];} col[i-10]=0;
				COL_XMB_TITLE=strtoul(col, NULL, 16);
				}

			if(strstr (line,"XMB_SUBTITLE=")!=NULL) { len = strlen(line)-2; 
				for(i = 13; i < len; i++) {col[i-13] = line[i];} col[i-13]=0;
				COL_XMB_SUBTITLE=strtoul(col, NULL, 16);
				}

			if(strstr (line,"XMB_SPARK_SIZE=")!=NULL) { len = strlen(line)-2; 
				for(i = 15; i < len; i++) {col[i-15] = line[i];} col[i-15]=0;
				XMB_SPARK_SIZE=strtoul(col, NULL, 10);
				}

			if(strstr (line,"XMB_SPARK_COLOR=")!=NULL) { len = strlen(line)-2; 
				for(i = 16; i < len; i++) {col[i-16] = line[i];} col[i-16]=0;
				XMB_SPARK_COLOR=strtoul(col, NULL, 16);
				}

			if(strstr (line,"device_list=0")!=NULL) th_device_list=0;
			if(strstr (line,"device_separator=0")!=NULL) th_device_separator=0;
			if(strstr (line,"legend=0")!=NULL) th_legend=0;
			if(strstr (line,"drive_icon=0")!=NULL) th_drive_icon=0;

			if(strstr (line,"device_separator_y=")!=NULL) {
				len = strlen(line)-2; for(i = 19; i < len; i++) {col[i-19] = line[i];} col[i-19]=0;
				th_device_separator_y=strtoul(col, NULL, 10);
			}

			if(strstr (line,"legend_y=")!=NULL) {
				len = strlen(line)-2; for(i = 9; i < len; i++) {col[i-9] = line[i];} col[i-9]=0;
				th_legend_y=strtoul(col, NULL, 10);
			}

			if(strstr (line,"drive_icon_x=")!=NULL) {
				len = strlen(line)-2; for(i = 13; i < len; i++) {col[i-13] = line[i];} col[i-13]=0;
				th_drive_icon_x=strtoul(col, NULL, 10);
			}

			if(strstr (line,"drive_icon_y=")!=NULL) {
				len = strlen(line)-2; for(i = 13; i < len; i++) {col[i-13] = line[i];} col[i-13]=0;
				th_drive_icon_y=strtoul(col, NULL, 10);
			}

			if(strstr (line,"user_font=")!=NULL) {
				len = strlen(line)-2; for(i = 10; i < len; i++) {col[i-10] = line[i];} col[i-10]=0;
				if(strtoul(col, NULL, 10)!=0) user_font=strtoul(col, NULL, 10);
				if(user_font<0 || user_font>9) user_font=1;
			}

			if(strstr (line,"game_bg_overlay=0")!=NULL) game_bg_overlay=0;
			if(strstr (line,"game_bg_overlay=1")!=NULL) game_bg_overlay=1;
		}
		fclose(fp);
	}

}

void write_last_state()
{

	char filename2[1024], filename3[64];
	sprintf(app_usrdir, "/dev_hdd0/game/%s/USRDIR",app_path);
	sprintf(filename2,  "%s/STATE.BIN", app_usrdir);

	if(stat(app_usrdir, &s3)<0) return;
	FILE *fpA;
//	remove(filename2);
	fpA = fopen ( filename2, "w" );
	char CrLf[2]; CrLf [0]=13; CrLf [1]=10; CrLf[2]=0;
	filename3[0]=0;
/*	sprintf(filename3, "fullpng=%i\r\n", cover_mode);			fputs (filename3,  fpA ); 
	sprintf(filename3, "display_mode=%i\r\n", display_mode);	fputs (filename3,  fpA ); 
	sprintf(filename3, "showdir=%i\r\n", dir_mode);				fputs (filename3,  fpA ); 
	sprintf(filename3, "overscan=%i\r\n", (int)(overscan*100));	fputs (filename3,  fpA ); 
*/
	sprintf(filename3, "game_sel=%i\r\n", game_sel);			fputs (filename3,  fpA ); 
	sprintf(filename3, "user_font=%i\r\n", user_font);			fputs (filename3,  fpA ); 
	fclose(fpA);
}

void parse_last_state()
{

	char string1[1024];
	char filename2[1024];
	sprintf(filename2,  "%s/STATE.BIN", app_usrdir);
	if(stat(filename2, &s3)<0) return;

	FILE *fp = fopen ( filename2, "r" );
	int i;


	if ( fp != NULL )
	{	
		char line [1024];
		while ( fgets ( line, sizeof line, fp ) != NULL ) /* read a line */
		{
			if(line[0]==35) continue;

/*	if(lock_display_mode==-1)
	{
		if(strstr (line,"fullpng=0")!=NULL) cover_mode=0;
		if(strstr (line,"fullpng=1")!=NULL) cover_mode=1;
		if(strstr (line,"fullpng=2")!=NULL) cover_mode=2;
		if(strstr (line,"fullpng=3")!=NULL) cover_mode=3;
		if(strstr (line,"fullpng=4")!=NULL) cover_mode=4;
		if(strstr (line,"fullpng=5")!=NULL) cover_mode=5;
		if(strstr (line,"fullpng=6")!=NULL) cover_mode=6;
		if(strstr (line,"fullpng=7")!=NULL) cover_mode=7;
		if(strstr (line,"fullpng=8")!=NULL) cover_mode=8;
	}

		if(strstr (line,"display_mode=0")!=NULL) {display_mode=0;}
		if(strstr (line,"display_mode=1")!=NULL) {display_mode=1;}
		if(strstr (line,"display_mode=2")!=NULL) {display_mode=2;}

		if(strstr (line,"showdir=0")!=NULL) {dir_mode=0;}
		if(strstr (line,"showdir=1")!=NULL) {dir_mode=1;}
		if(strstr (line,"showdir=2")!=NULL) {dir_mode=2;}

*/
		if(strstr (line,"user_font=")!=NULL) {if(line[10]!=0x30) user_font=line[10]-0x30;}

		if(strstr (line,"game_sel=")!=NULL) {
			int len = strlen(line)-2; for(i = 9; i < len; i++) {string1[i-9] = line[i];} string1[i-9]=0;
			game_sel=strtol(string1, NULL, 10);
			if(game_sel<0 || game_sel>max_menu_list-1) game_sel=0;
		}


/*		if(strstr (line,"overscan=")!=NULL) {
			int len = strlen(line)-2; for(i = 9; i < len; i++) {overscanS[i-9] = line[i];} overscanS[i-9]=0;
			if(overscanS[1]==0 && overscanS[0]!=0) overscan=(float) (overscanS[0]-48)/100.0f;
			else
			if(overscanS[2]==0 && overscanS[1]!=0 && overscanS[0]!=0) overscan=(float) ((overscanS[0]-48)*10.0f + (overscanS[1]-48))/100.0f;
			}
			if(overscan>0.12f) overscan=0.12f;
			if(overscan<-0.06f) overscan=-0.06f;
			overscanX=(int) (1920*overscan);
			overscanY=(int) (1080*overscan);
			int overscanP=(int) (overscan*100);

			if(overscanP>0 && overscanP<16)
			{
				sprintf(overscan_head,"               ");
				overscan_head[overscanP]=0;
			}
			else
				overscan_head[0]=0;
*/


		}

	fclose ( fp );
	}
}

int parse_ini(char * file, int skip_bin)
{

	int i;

	max_hosts=0;
	game_last_page=-1; old_fi=-1;
	char line [ 256 ];

	FILE *fp;

	if(stat(file, &s3)<0) goto op_bin;

	fp = fopen ( file, "r" );
if ( fp != NULL )
{

	while ( fgets ( line, sizeof line, fp ) != NULL ) /* read a line */
	{
		if(line[0]==35) continue;

		if(strstr (line,"hdd_dir=")!=NULL) {
			int len = strlen(line)-2; for(i = 8; i < len; i++) {ini_hdd_dir[i-8] = line[i];} ini_hdd_dir[i-8]=0;
//			DPrintf("Game backup folder (HDD): [%s]\n", ini_hdd_dir);
			}

		if(strstr (line,"usb_dir=")!=NULL) {
			int len = strlen(line)-2; for(i = 8; i < len; i++) {ini_usb_dir[i-8] = line[i];} ini_usb_dir[i-8]=0;
//			DPrintf("Game backup folder (USB): [%s]\n", ini_usb_dir);
			}

		if(strstr (line,"hdd_home=")!=NULL) {
			int len = strlen(line)-2; for(i = 9; i < len; i++) {hdd_home[i-9] = line[i];} hdd_home[i-9]=0;
//			DPrintf("Game search folder (HDD): [%s]\n", hdd_home);
			}

		if(strstr (line,"hdd_home2=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {hdd_home_2[i-10] = line[i];} hdd_home_2[i-10]=0;
//			DPrintf("Game search folder (HDD) (aux#1): [%s]\n", hdd_home_2);
			}

		if(strstr (line,"hdd_home3=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {hdd_home_3[i-10] = line[i];} hdd_home_3[i-10]=0;
//			DPrintf("Game search folder (HDD) (aux#2): [%s]\n", hdd_home_3);
			}

		if(strstr (line,"hdd_home4=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {hdd_home_4[i-10] = line[i];} hdd_home_4[i-10]=0;
//			DPrintf("Game search folder (HDD) (aux#3): [%s]\n", hdd_home_4);
			}

		if(strstr (line,"hdd_home5=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {hdd_home_5[i-10] = line[i];} hdd_home_5[i-10]=0;
//			DPrintf("Game search folder (HDD) (aux#4): [%s]\n", hdd_home_5);
			}

		if(strstr (line,"usb_home=")!=NULL) {
			int len = strlen(line)-2; for(i = 9; i < len; i++) {usb_home[i-9] = line[i];} usb_home[i-9]=0;
//			DPrintf("Game search folder (USB): [%s]\n", usb_home);
			}

		if(strstr (line,"usb_home2=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {usb_home_2[i-10] = line[i];} usb_home_2[i-10]=0;
//			DPrintf("Game search folder (USB) (aux#1): [%s]\n", usb_home_2);	
			}

		if(strstr (line,"usb_home3=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {usb_home_3[i-10] = line[i];} usb_home_3[i-10]=0;
//			DPrintf("Game search folder (USB) (aux#2): [%s]\n", usb_home_3);	
			}

		if(strstr (line,"usb_home4=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {usb_home_4[i-10] = line[i];} usb_home_4[i-10]=0;
//			DPrintf("Game search folder (USB) (aux#3): [%s]\n", usb_home_4);	
			}

		if(strstr (line,"usb_home5=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {usb_home_5[i-10] = line[i];} usb_home_5[i-10]=0;
//			DPrintf("Game search folder (USB) (aux#4): [%s]\n", usb_home_5);	
			}

		if(strstr (line,"covers_dir=")!=NULL) {
			int len = strlen(line)-2; for(i = 11; i < len; i++) {covers_dir[i-11] = line[i];} covers_dir[i-11]=0;
//			DPrintf("Game covers folder (HDD): [%s]\n", covers_dir);
			}

		if(strstr (line,"themes_dir=")!=NULL) {
			int len = strlen(line)-2; for(i = 11; i < len; i++) {themes_dir[i-11] = line[i];} themes_dir[i-11]=0;
//			DPrintf("Themes folder: [%s]\n", themes_dir);
			}

		if(strstr (line,"themes_web_dir=")!=NULL) {
			int len = strlen(line)-2; for(i = 15; i < len; i++) {themes_web_dir[i-15] = line[i];} themes_web_dir[i-15]=0;
//			DPrintf("Themes folder: [%s]\n", themes_dir);
			}


		if(strstr (line,"update_dir=")!=NULL) {
			int len = strlen(line)-2; for(i = 11; i < len; i++) {update_dir[i-11] = line[i];} update_dir[i-11]=0;
//			DPrintf("Update save folder: [%s]\n", update_dir);
			}

		if(strstr (line,"download_dir=")!=NULL) {
			int len = strlen(line)-2; for(i = 13; i < len; i++) {download_dir[i-13] = line[i];} download_dir[i-13]=0;
//			DPrintf("Web browser downloads save folder: [%s]\n", download_dir);
			}

		if(strstr (line,"snes_roms=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {snes_roms[i-10] = line[i];} snes_roms[i-10]=0;
			}

		if(strstr (line,"snes_self=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {snes_self[i-10] = line[i];} snes_self[i-10]=0;
			}

		if(strstr (line,"genp_roms=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {genp_roms[i-10] = line[i];} genp_roms[i-10]=0;
			}

		if(strstr (line,"genp_self=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {genp_self[i-10] = line[i];} genp_self[i-10]=0;
			}

		if(strstr (line,"fceu_roms=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {fceu_roms[i-10] = line[i];} fceu_roms[i-10]=0;
			}

		if(strstr (line,"fceu_self=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {fceu_self[i-10] = line[i];} fceu_self[i-10]=0;
			}

		if(strstr (line,"vba_roms=")!=NULL) {
			int len = strlen(line)-2; for(i = 9; i < len; i++) {vba_roms[i-9] = line[i];} vba_roms[i-9]=0;
			}

		if(strstr (line,"vba_self=")!=NULL) {
			int len = strlen(line)-2; for(i = 9; i < len; i++) {vba_self[i-9] = line[i];} vba_self[i-9]=0;
			}


    //nethost:10.20.2.208:11222:/downloads/
	char n_prefix[32], n_host[128], n_port[6], n_friendly[64], n_em[64];
	if(sscanf(line,"%[^*]*%[^*]*%[^*]*%[^*]*%s\n", n_prefix, n_host, n_port, n_friendly, n_em)>=4) //, n_root //%[^:]:
		if(strcmp(n_prefix, "nethost")==0 && max_hosts<9)
		{ 
			sprintf(host_list[max_hosts].host, "%s", n_host);
			host_list[max_hosts].port=strtol(n_port, NULL, 10);
//			sprintf(host_list[max_hosts].root, "%s", n_root);
			sprintf(host_list[max_hosts].friendly, "%s", n_friendly);
			sprintf(host_list[max_hosts].name,"%s/net_host%i", app_usrdir, max_hosts);
			DPrintf("[Host PC#%i] %s:%s * Friendly name: %s\n", max_hosts+1, n_host, n_port, n_friendly); //, n_root // Serve path: %s 
			max_hosts++;
		}

	}

	fclose ( fp );

// if(strstr(mp3_now_playing, "SOUND.BIN")==NULL)  DPrintf("Now playing: %s", mp3_now_playing );

}

op_bin:
	if(skip_bin) goto out_ini;

	fp = fopen ( options_bin, "r" );
if ( fp != NULL )
{

	while ( fgets ( line, sizeof line, fp ) != NULL ) /* read a line */
	{
		if(line[0]==35) continue;

		if(strstr (line,"ftpd_on=1")!=NULL) {ftp_on(); ftp_service=1;}

//		int bpcm=cover_mode;
		if(strstr (line,"fullpng=0")!=NULL) cover_mode=0;
		if(strstr (line,"fullpng=1")!=NULL) cover_mode=1;
		if(strstr (line,"fullpng=2")!=NULL) cover_mode=2;
		if(strstr (line,"fullpng=3")!=NULL) cover_mode=3;
		if(strstr (line,"fullpng=4")!=NULL) cover_mode=4;
		if(strstr (line,"fullpng=6")!=NULL) cover_mode=6;
		if(strstr (line,"fullpng=5")!=NULL) cover_mode=5;
		if(strstr (line,"fullpng=7")!=NULL) cover_mode=7;
		if(strstr (line,"fullpng=8")!=NULL) cover_mode=8;

		initial_cover_mode=cover_mode;

		if(strstr (line,"game_bg_overlay=0")!=NULL) game_bg_overlay=0;
		if(strstr (line,"game_bg_overlay=1")!=NULL) game_bg_overlay=1;

		if(strstr (line,"gray_poster=0")!=NULL) gray_poster=0;
		if(strstr (line,"gray_poster=1")!=NULL) gray_poster=1;

		if(strstr (line,"confirm_with_x=0")!=NULL) confirm_with_x=0;
		if(strstr (line,"confirm_with_x=1")!=NULL) confirm_with_x=1;
		set_xo();

		if(strstr (line,"hide_bd=0")!=NULL) hide_bd=0;
		if(strstr (line,"hide_bd=1")!=NULL) hide_bd=1;

		if(strstr (line,"theme_sound=0")!=NULL) theme_sound=0;
		if(strstr (line,"theme_sound=1")!=NULL) theme_sound=1;

		if(strstr (line,"display_mode=0")!=NULL && line[0]=='d') display_mode=0;
		if(strstr (line,"display_mode=1")!=NULL && line[0]=='d') display_mode=1;
		if(strstr (line,"display_mode=2")!=NULL && line[0]=='d') display_mode=2;

		if(strstr (line,"showdir=0")!=NULL) dir_mode=0;
		if(strstr (line,"showdir=1")!=NULL) dir_mode=1;
		if(strstr (line,"showdir=2")!=NULL) dir_mode=2;

		if(strstr (line,"game_details=0")!=NULL) game_details=0;
		if(strstr (line,"game_details=1")!=NULL) game_details=1;
		if(strstr (line,"game_details=2")!=NULL) game_details=2;
		if(strstr (line,"game_details=3")!=NULL) game_details=3;

		if(strstr (line,"bd_emulator=0")!=NULL) bd_emulator=0;
		if(strstr (line,"bd_emulator=1")!=NULL) bd_emulator=1;
		if(strstr (line,"bd_emulator=2")!=NULL) bd_emulator=2;
		if(c_firmware==3.41f && bd_emulator>1) bd_emulator=1;

		if(strstr (line,"scan_avchd=0")!=NULL) scan_avchd=0;
		if(strstr (line,"scan_avchd=1")!=NULL) scan_avchd=1;

		if(strstr (line,"clear_activity_logs=0")!=NULL) clear_activity_logs=0;
		if(strstr (line,"clear_activity_logs=1")!=NULL) clear_activity_logs=1;

//		if(strstr (line,"load_last_state=0")!=NULL) load_last_state=0;
//		if(strstr (line,"load_last_state=1")!=NULL) load_last_state=1;

		if(strstr (line,"lock_display_mode=0")!=NULL) lock_display_mode=0;
		if(strstr (line,"lock_display_mode=1")!=NULL) lock_display_mode=1;
		if(strstr (line,"lock_display_mode=2")!=NULL) lock_display_mode=2;
		if(strstr (line,"lock_display_mode=3")!=NULL) lock_display_mode=3;
		if(strstr (line,"lock_display_mode=4")!=NULL) lock_display_mode=4;
		if(strstr (line,"lock_display_mode=5")!=NULL) lock_display_mode=5;
		if(strstr (line,"lock_display_mode=6")!=NULL) lock_display_mode=6;
		if(strstr (line,"lock_display_mode=7")!=NULL) lock_display_mode=7;
		if(strstr (line,"lock_display_mode=8")!=NULL) lock_display_mode=8;

		if(strstr (line,"expand_avchd=0")!=NULL) expand_avchd=0;
		if(strstr (line,"expand_avchd=1")!=NULL) expand_avchd=1;

		if(strstr (line,"lock_fileman=0")!=NULL) lock_fileman=0;
		if(strstr (line,"lock_fileman=1")!=NULL) lock_fileman=1;

		if(strstr (line,"progress_bar=0")!=NULL) progress_bar=0;
		if(strstr (line,"progress_bar=1")!=NULL) progress_bar=1;

		if(strstr (line,"verify_data=0")!=NULL) verify_data=0;
		if(strstr (line,"verify_data=1")!=NULL) verify_data=1;
		if(strstr (line,"verify_data=2")!=NULL) verify_data=2;

		if(strstr (line,"scan_for_apps=0")!=NULL) scan_for_apps=0;
		if(strstr (line,"scan_for_apps=1")!=NULL) scan_for_apps=1;

		if(strstr (line,"xmb_sparks=0")!=NULL) xmb_sparks=0;
		if(strstr (line,"xmb_sparks=1")!=NULL) xmb_sparks=1;
		if(strstr (line,"xmb_sparks=2")!=NULL) xmb_sparks=2;

		if(strstr (line,"xmb_popup=0")!=NULL) xmb_popup=0;
		if(strstr (line,"xmb_popup=1")!=NULL) xmb_popup=1;

		if(strstr (line,"xmb_game_bg=0")!=NULL) xmb_game_bg=0;
		if(strstr (line,"xmb_game_bg=1")!=NULL) xmb_game_bg=1;

		if(strstr (line,"xmb_cover=0")!=NULL) xmb_cover=0;
		if(strstr (line,"xmb_cover=1")!=NULL) xmb_cover=1;

		if(strstr (line,"date_format=0")!=NULL) date_format=0;
		if(strstr (line,"date_format=1")!=NULL) date_format=1;
		if(strstr (line,"date_format=2")!=NULL) date_format=2;

		if(strstr (line,"time_format=0")!=NULL) time_format=0;
		if(strstr (line,"time_format=1")!=NULL) time_format=1;

		if(strstr (line,"mount_hdd1=0")!=NULL) mount_hdd1=0;
		if(strstr (line,"mount_hdd1=1")!=NULL) mount_hdd1=1;

		if(strstr (line,"animation=0")!=NULL) animation=0;
		if(strstr (line,"animation=1")!=NULL) animation=1;
		if(strstr (line,"animation=2")!=NULL) animation=2;
		if(strstr (line,"animation=3")!=NULL) animation=3;

		if(strstr (line,"disable_options=0")!=NULL) disable_options=0;// DPrintf("Disable options: [none]\n");}
		if(strstr (line,"disable_options=1")!=NULL) disable_options=1;// DPrintf("Disable options: [delete]\n");}
		if(strstr (line,"disable_options=2")!=NULL) disable_options=2;// DPrintf("Disable options: [copy/backup]\n");}
		if(strstr (line,"disable_options=3")!=NULL) disable_options=3;// DPrintf("Disable options: [copy/backup/delete]\n");}

		if(strstr (line,"download_covers=0")!=NULL) download_covers=0;
		if(strstr (line,"download_covers=1")!=NULL) download_covers=1;

		if(strstr (line,"overscan=")!=NULL) {
			overscan=(strtod(((char*)line)+9, NULL)/100.f);
			if(overscan>0.10f) overscan=0.10f;
			if(overscan<0.00f) overscan=0.00f;
		}

		char dimS[8];
		if(strstr (line,"dim_titles=")!=NULL) {
			int len = strlen(line)-2; for(i = 11; i < len; i++) {dimS[i-11] = line[i];} dimS[i-11]=0;
			dim_setting=strtoul(dimS, NULL, 10);
			if(dim_setting>10) dim_setting=10;
		}

		if(strstr (line,"ss_timeout=")!=NULL) {
			int len = strlen(line)-2; for(i = 11; i < len; i++) {dimS[i-11] = line[i];} dimS[i-11]=0;
			ss_timeout=strtoul(dimS, NULL, 10);
			if(ss_timeout>10) ss_timeout=0;
		}

		if(strstr (line,"user_font=")!=NULL) {
			int len = strlen(line)-2; for(i = 10; i < len; i++) {dimS[i-10] = line[i];} dimS[i-10]=0;
			user_font=strtoul(dimS, NULL, 10);
			if(user_font<0 || user_font>9) user_font=0;
		}

		if(strstr (line,"deadzone_x=")!=NULL) {
			int len = strlen(line)-2; for(i = 11; i < len; i++) {dimS[i-11] = line[i];} dimS[i-11]=0;
			xDZ=strtoul(dimS, NULL, 10);
			if(xDZ>90) xDZ=90;
			xDZa=(int) (xDZ*128/100);
		}

		if(strstr (line,"deadzone_y=")!=NULL) {
			int len = strlen(line)-2; for(i = 11; i < len; i++) {dimS[i-11] = line[i];} dimS[i-11]=0;
			yDZ=strtoul(dimS, NULL, 10);
			if(yDZ>90) yDZ=90;
			yDZa=(int) (yDZ*128/100);
		}


		if(strstr (line,"parental_level=")!=NULL) {
			int len = strlen(line)-2; for(i = 15; i < len; i++) {dimS[i-15] = line[i];} dimS[i-15]=0;
			parental_level=strtoul(dimS, NULL, 10);
			if(parental_level<0 || parental_level>11) parental_level=0;
		}

		if(strstr (line,"parental_pass=")!=NULL) {
			int len = strlen(line)-2; for(i = 14; i < len; i++) {dimS[i-14] = line[i];} dimS[i-14]=0;
			strncpy(parental_pass, dimS, 4); parental_pass[4]=0;
			if(strlen(parental_pass)<4) {sprintf(parental_pass, "0000"); parental_pass[4]=0;}
		}
	}

	fclose ( fp );

}

out_ini:

return 0; 
}


void get_www_themes(theme_def *list, u8 *max)
{		int ret=0;
		(*max)=0;

		if(cellNetCtlGetInfo(16, &net_info)<0)

		{
			//net_avail=-1;
			dialog_ret=0;
			ret = cellMsgDialogOpen2( type_dialog_ok, "Internet connection is not available or an error has occured!", dialog_fun2, (void*)0x0000aaab, NULL );					
			wait_dialog();
			return;
		}

		//dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
		//flipc(60);

		char filename[1024];

		char update_url[128]=" ";
		char local_file[64]=" ";
		char line[2048];

		char update_server[256];
		sprintf(update_server, "%s/themes_web/", url_base);

		if(c_firmware>3.30f) sprintf(update_url,"%sthemes.bin", update_server);
		if(c_firmware<3.40f) sprintf(update_url,"%sthemes315.bin", update_server);
		if(c_firmware>3.54f) sprintf(update_url,"%sthemes355.bin", update_server);

		sprintf(local_file, "%s/themes_check.bin", app_temp);
		remove(local_file);
		ret = download_file(update_url, local_file, 0);
		cellMsgDialogAbort();
		if(ret==0) 		{
			dialog_ret=0;
			//net_avail=-1;
			ret = cellMsgDialogOpen2( type_dialog_ok, "Internet connection is not available or an error has occured!", dialog_fun2, (void*)0x0000aaab, NULL );					
			wait_dialog();
			remove(local_file);
//			sprintf(local_file, "%s/themes_check.bin", app_temp);
//			sprintf(line, "%s/THEME.BIN"
//			remove(local_file);
			return;
		}

		ret=0;

		fpV = fopen ( local_file, "r" );
		if ( fpV != NULL ) 
		{

			char t_prefix[8], t_name[64], t_pkg[512], t_img[512], t_author[32], t_ver[16], t_free[64], t_em[128];

			while ( fgets ( line, sizeof line, fpV ) != NULL )
			{
				if(sscanf(line,"%[^*]*%[^*]*%[^*]*%[^*]*%[^*]*%[^*]*%[^*]*%s\n", t_prefix, t_name, t_pkg, t_img, t_author, t_ver, t_free, t_em)>=7)
					if(strcmp(t_prefix, "theme")==0 && (*max)< (MAX_WWW_THEMES-1))
					{ 
						sprintf(list[*max].name, "%s", t_name);
						sprintf(list[*max].pkg, "%s", t_pkg);
						sprintf(list[*max].img, "%s", t_img);
						sprintf(list[*max].author, "%s", t_author);
						sprintf(list[*max].mmver, "%s", t_ver);
						sprintf(list[*max].info, "%s", t_free);

//			dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, t_name, dialog_fun2, (void*)0x0000aaab, NULL );wait_dialog();
//			dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, t_pkg, dialog_fun2, (void*)0x0000aaab, NULL );wait_dialog();
						(*max)++;
					}
			}
			fclose(fpV);
		}

		 if ((*max)==0)
		 {
				sprintf(filename, "Error occured while contacting the server!\n\nPlease try again later."); 
				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();
		 }

 		remove(local_file);

}


void check_for_update()
{		int ret=0;

		force_update_check=0;
		if(cellNetCtlGetInfo(16, &net_info)<0)//net_avail<0 || 

		{
			net_avail=-1;
			dialog_ret=0;
			ret = cellMsgDialogOpen2( type_dialog_ok, "Internet connection is not available or an error has occured!", dialog_fun2, (void*)0x0000aaab, NULL );					
			wait_dialog();
			return;
		}

		dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
		flipc(60);

		char filename[1024];

		char update_url[128]=" ";
		char local_file[64]=" ";
		char line[128];
		char usb_save[128]="/skip";

		if(stat(update_dir, &s3)>=0)
			sprintf(usb_save, "%s", update_dir);

		else

			for(int n=0;n<9;n++){
				sprintf(filename, "/dev_usb00%i", n);
				if(stat(filename, &s3)>=0) {
					sprintf(usb_save, "%s", filename);
					break;
				}
			}
//		}

		if(stat(usb_save, &s3)<0 && payload>-1) sprintf(usb_save,"%s/TEMP", app_usrdir); // && c_firmware<3.55f

		char update_server[256];
		sprintf(update_server, "%s/", url_base);

		if(c_firmware>3.30f) sprintf(update_url,"%sversion.txt", update_server);
		if(c_firmware<3.40f) sprintf(update_url,"%sversion315.txt", update_server);
		if(c_firmware>3.54f) sprintf(update_url,"%sversion355.txt", update_server);

		sprintf(local_file, "%s", versionUP);
		remove(local_file);
		ret = download_file(update_url, local_file, 0);
		cellMsgDialogAbort();
		if(ret==0) 		{
			dialog_ret=0;
			net_avail=-1;
			ret = cellMsgDialogOpen2( type_dialog_ok, "Internet connection is not available or an error has occured!", dialog_fun2, (void*)0x0000aaab, NULL );					
			wait_dialog();
			return;
		}

		char new_version[9]; ret=0;

		fpV = fopen ( versionUP, "r" );
		if ( fpV != NULL ) 
			{
				while ( fgets ( line, sizeof line, fpV ) != NULL )
					{
						if(strlen(line)==8) {sprintf(new_version, "%s", line); ret=1;}
						break;
					}
				fclose(fpV);
			}


		if (ret==1)
		{

				char whatsnew[512]; whatsnew[0]=0;

				sprintf(local_file, "%s/whatsnew.txt", app_temp); remove(local_file);
				sprintf(update_url,"%swn.txt", update_server);
				ret = download_file(update_url, local_file, 0);
				if (ret==1)
				{

					fpV = fopen ( local_file, "r" );
					if ( fpV != NULL ) 
					{
						while ( fgets ( line, sizeof line, fpV ) != NULL )
						{
							sprintf(whatsnew, "%s%s", whatsnew, line);
						}
						fclose(fpV);
						remove(local_file);

					whatsnew[511]=0;
					sprintf(filename, "What's new in multiMAN %s:\n\n%s", new_version, whatsnew); 
					dialog_ret=0;
					ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );					
					wait_dialog();

					}
				}

		if(stat(usb_save, &s3)>=0)
		{

			if(strcmp(current_version, new_version)!=0)
			{


//				if(c_firmware>3.30f) sprintf(filename, "New version found (FW 3.40-3.42): %s\n\nYour current version: %s\n\nDo you want to download the update?", new_version, current_version); 
//				if(c_firmware<3.40f) sprintf(filename, "New version found: %s\n\nYour current version: %s\n\nDo you want to download the update?", new_version, current_version); 
//				if(c_firmware>3.30f) 
				sprintf(filename, "New version found: %s\n\nYour current version: %s\n\nDo you want to download the update?", new_version, current_version); 

				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				wait_dialog();
				if(dialog_ret==1) {

				if(c_firmware<=3.30f) sprintf(update_url,"%smultiMAN2_315.bin", update_server);
				if(c_firmware>3.54f) sprintf(update_url,"%smultiMAN2_355.bin", update_server);
				if(c_firmware>3.39f) sprintf(update_url,"%smultiMAN2_340.bin", update_server);


					sprintf(local_file,"%s/multiMAN_%s.pkg", usb_save, new_version);
					ret = download_file(update_url, local_file, 1);
					dialog_ret=0;
					if(ret==1) 
						{
							if(strstr(local_file, "USRDIR/TEMP")!=NULL) sprintf(local_file, "/app_home/multiMAN_%s.pkg", new_version);
							sprintf(filename, "Download completed successfully!\n\nInstall the update from [* Install Package Files] XMB\xE2\x84\xA2 tab.\n\nUpdate file saved as: %s\n\nQuit to XMB\xE2\x84\xA2 now?", local_file); 
							ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );					
							wait_dialog();
							if(dialog_ret==1)
								if(net_used_ignore()){
								syscall_mount2( (char *) "/app_home", (char *) usb_save);
								unload_modules(); 
								sys_process_exit(1);
							}
						}
					else
						{
							ret = cellMsgDialogOpen2( type_dialog_ok, "Error occured while downloading the update!\n\nPlease try again later.", dialog_fun2, (void*)0x0000aaab, NULL );					
							wait_dialog();
						}
				}
			}
			else
			{
				sprintf(filename, "You already have the latest version: %s\n\n            There is no need to update.", current_version); 
				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();
			}

		} 
	else //no usb/card connected
			{
				sprintf(filename, "Please attach USB storage device to save update data and try again!"); 
				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();
			} 



		}
		else
		 {
				sprintf(filename, "Error occured while contacting the update server!\n\nPlease try again later."); 
				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();
		 }
 		remove(versionUP);


}

void check_for_game_update(char *game_id, char *game_title)
{		int ret=0;

		if(net_avail<0 || cellNetCtlGetInfo(16, &net_info)<0)
		{
			dialog_ret=0;
			net_avail=-1;
			ret = cellMsgDialogOpen2( type_dialog_ok, "Please enable [Internet Connection] from XMB\xE2\x84\xA2 [Settings] tab!", dialog_fun2, (void*)0x0000aaab, NULL );					
			wait_dialog();
			return;
		}
		char filename[1024];

		char update_url[256]=" ";
		char local_file[512]=" ";
		char line[512];
		char usb_save[512]="/skip";
		char versionGAME[256];
		char temp_val[32];
		float param_ver=0.0f;
		char game_param_sfo[512];
		sprintf(game_param_sfo, "/dev_hdd0/game/%s/PARAM.SFO", game_id);

if(get_param_sfo_field((char *)game_param_sfo, (char *)"APP_VER", (char *)temp_val))
	{
				param_ver=strtof(temp_val, NULL);
	}

		typedef struct
		{
			char pkg_ver[8];
			uint64_t pkg_size;
			char ps3_ver[8];
			char pkg_url[1024];
		}
		pkg_update;
		pkg_update pkg_list[32]; 
		int max_pkg=0;


		if(stat(update_dir, &s3)>=0)
			sprintf(usb_save, "%s", update_dir);

		else

			for(int n=0;n<9;n++){
				sprintf(filename, "/dev_usb00%i", n);
				if(stat(filename, &s3)>=0) {
					sprintf(usb_save, "%s", filename);
					break;
				}
			}
//		}

		if(stat(usb_save, &s3)<0 && payload>-1) 
		{
			sprintf(usb_save,"%s/PKG", app_usrdir); 			
			mkdir(usb_save, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
		}
		sprintf(versionGAME,"%s/TEMP/%s.UPD", app_usrdir, game_id);

		sprintf(update_url, "%s/ps3u/?ID=%s", url_base, game_id);

		remove(versionGAME);
		ret = download_file(update_url, versionGAME, 0);
		char new_version[511]; new_version[0]=0; ret=0;
		char g_title[256], g_ver[256], g_url[1024], g_ps3[8];
		sprintf(g_title, "%s", game_title);
		uint64_t all_pkg=0;//g_size=0, 
		int lc=0, first_pkg=-1;

		fpV = fopen ( versionGAME, "r" );
		if ( fpV != NULL ) 
			{
				while ( fscanf (fpV, "%[^|]|%[^|]|%[^|]|%s\n", g_ver, line, g_ps3, g_url )>3)
					{
						lc++;
						if(lc==1 && strlen(g_ver)>2) sprintf(g_title, "[%s]: %s", game_id, g_ver);
						if(lc>1)
						{
							sprintf(pkg_list[max_pkg].pkg_ver, "%s", g_ver);	
							sprintf(pkg_list[max_pkg].ps3_ver, "%s", g_ps3);
							sprintf(pkg_list[max_pkg].pkg_url, "%s", g_url);
							pkg_list[max_pkg].pkg_size = strtoull(line, NULL, 10); 
							all_pkg+=pkg_list[max_pkg].pkg_size;
							if(param_ver<strtof(g_ver, NULL) && first_pkg==-1) first_pkg=max_pkg;
							max_pkg++;

						}
					}
				fclose(fpV);
			}

		if (max_pkg>0)
		{
			if(param_ver>=strtof(pkg_list[max_pkg-1].pkg_ver, NULL)) first_pkg=max_pkg;
			if(param_ver==0.0f) first_pkg=-1;
			if(max_pkg==1)
				sprintf(filename, "%s\n\nVersion: %s (%i update)\nUpdate size: %.2fMB\n\nDownload update now?", g_title, pkg_list[0].pkg_ver, max_pkg, (double)(all_pkg/1024/1024)); 
			else
				sprintf(filename, "%s\n\nVersions: %s - %s (%i updates)\nTotal update size: %.2fMB\n\nDownload updates now?", g_title, pkg_list[0].pkg_ver, pkg_list[max_pkg-1].pkg_ver, max_pkg, (double)(all_pkg/1024/1024)); 

				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				wait_dialog();

		if( (first_pkg!=-1) && dialog_ret==1)
		{

				sprintf(filename, "%s\n\nYou already have version %.2f installed.\n\nDo you want to download newer updates only?", g_title, param_ver ); 
				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				wait_dialog();
				if(dialog_ret!=1) first_pkg=0;
				if(dialog_ret==3) goto just_quit;
				dialog_ret=1;

		}
		if(first_pkg<0) first_pkg=0;

		if(dialog_ret==1)
		{
			if(stat(usb_save, &s3)>=0)
			{	int lc2;
				ret=0;
				for(lc=first_pkg;lc<max_pkg;lc++)
				{
					sprintf(local_file, "%s/%s_[UPDATE_%02i_PS3FW_%s]_VER_%s.pkg", usb_save, game_id, lc+1, pkg_list[lc].ps3_ver, pkg_list[lc].pkg_ver);
					ret=0;
					for(lc2=0;lc2<3;lc2++){
						ret = download_file(pkg_list[lc].pkg_url, local_file, 1);
						if(ret>0) break;
					}
					if(ret!=1) break;
				}
					dialog_ret=0;
					if(ret==1) 
						{
						if(max_pkg==1)
							sprintf(filename, "Download completed successfully!\n\nInstall the update from [* Install Package Files] XMB\xE2\x84\xA2 tab.\n\nUpdate file saved as: %s\n\nQuit to XMB\xE2\x84\xA2 now?", local_file); 
						else
							sprintf(filename, "Download completed successfully!\n\nUpdate files saved in: %s\n\nQuit to XMB\xE2\x84\xA2 now?", usb_save); 
							ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );					
							wait_dialog();
							if(dialog_ret==1) 
								if(net_used_ignore()) {
								syscall_mount2( (char *) "/app_home", (char *) usb_save);
								unload_modules(); 
								sys_process_exit(1);
							}
						}
					else
						{
							if(first_pkg>=max_pkg)
								ret = cellMsgDialogOpen2( type_dialog_ok, "You already have the latest version of the game.", dialog_fun2, (void*)0x0000aaab, NULL );					
							else
								ret = cellMsgDialogOpen2( type_dialog_ok, "Error occured while downloading the update!\n\nPlease try again later.", dialog_fun2, (void*)0x0000aaab, NULL );					
							wait_dialog();
						}
			}
			else //no usb/card connected
			{
				sprintf(filename, "Please attach USB storage device to save update data and try again!"); 
				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();
			} 

		} 

		}
		else
		 {
				sprintf(filename, "Cannot find update information for this game!"); 
				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();
		 }
just_quit:
	ret=1;
}

void clean_up()
{
	char cleanup[48]=" ";
	if(clear_activity_logs==1)
	{
		sprintf(cleanup, "/dev_hdd0/vsh/pushlist/patch.dat"); remove(cleanup);
		sprintf(cleanup, "/dev_hdd0/vsh/pushlist/game.dat"); remove(cleanup);
		for(int n2=0;n2<20;n2++) {sprintf(cleanup, "/dev_hdd0/home/000000%02i/etc/boot_history.dat", n2); remove(cleanup);}
	}
	for(int n2=0;n2<20;n2++) {
		sprintf(cleanup, "/dev_hdd0/home/000000%02i", n2); cellFsChmod(cleanup, CELL_FS_S_IFDIR | 0777);
		sprintf(cleanup, "/dev_hdd0/home/000000%02i/savedata", n2); cellFsChmod(cleanup, CELL_FS_S_IFDIR | 0777);
		}
	sprintf(cleanup, "%s", "/dev_hdd0/game"); cellFsChmod(cleanup, CELL_FS_S_IFDIR | 0777);
}

void slide_xmb_left(int _xmb_icon)
{
	xmb_sublevel=0;
	xmb_slide=0;
	xmb_slide_step=-15;

	for(int n=0; n<14; n++)
	{
		xmb_slide+=xmb_slide_step;
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		if(xmb_sparks!=0) draw_stars();

		set_texture( text_bmp, 1920, 1080);
		display_img(0, 0, 1920, 1080, 1920, 1080, 0.9f, 1920, 1080);

		draw_xmb_clock(xmb_clock, 0);
		draw_xmb_icons(xmb, xmb_icon, xmb_slide, xmb_slide_y, 0, xmb_sublevel);
	
		flip();
		//cellSysutilCheckCallback();
	}
	xmb_sublevel=1;
	xmb_slide=0;
	xmb_slide_step=0;
	draw_xmb_bare(_xmb_icon, 1, 0, 1);
}

void slide_xmb_right()
{
		xmb_slide=0;
		xmb_slide_step=15;
		xmb_sublevel=1;
		for(int n=0; n<14; n++)
		{
			xmb_slide+=xmb_slide_step;
			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			if(xmb_sparks!=0) draw_stars();

			set_texture( text_bmp, 1920, 1080);
			display_img(0, 0, 1920, 1080, 1920, 1080, 0.9f, 1920, 1080);

			draw_xmb_clock(xmb_clock, 0);
			draw_xmb_icons(xmb, xmb_icon, xmb_slide, xmb_slide_y, 0, xmb_sublevel);
		
			flip();
			//cellSysutilCheckCallback();
		}

		xmb_slide=0;
		xmb_slide_step=0;
		xmb_sublevel=0;
}

void select_theme()
{
		slide_xmb_left(1);
		t_dir_pane_bare *pane =  (t_dir_pane_bare *) malloc(sizeof(t_dir_pane_bare)*MAX_PANE_SIZE_BARE);
		int max_dir=0;
		ps3_home_scan_ext_bare(themes_dir, pane, &max_dir, (char*)".mmt"); 
		opt_list_max=0;
		for(int n=0; n<max_dir; n++)
		{
			if(pane[n].name[0]=='_') 
				sprintf(opt_list[opt_list_max].label, "%s", pane[n].name+1);
			else
				sprintf(opt_list[opt_list_max].label, "%s", pane[n].name);

			opt_list[opt_list_max].label[strlen(opt_list[opt_list_max].label)-4]=0;
			sprintf(opt_list[opt_list_max].value, "%s", pane[n].path);
			opt_list_max++;
			if(opt_list_max>=MAX_LIST_OPTIONS) break;
		}
		if(opt_list_max)
		{
			use_analog=1;
			float b_mX=mouseX;
			float b_mY=mouseY;
			mouseX=660.f/1920.f;
			mouseY=225.f/1080.f;
			is_any_xmb_column=xmb_icon;
			int ret_f=open_list_menu((char*) "Select Theme", 600, opt_list, opt_list_max, 660, 225, 16, 1);
			is_any_xmb_column=0;
			use_analog=0;
			mouseX=b_mX;
			mouseY=b_mY;
			if(ret_f!=-1) 
			{
				char tmp_thm[64];
				sprintf(tmp_thm, "skip/_%s.mmt", opt_list[ret_f].label);
				apply_theme(tmp_thm, opt_list[ret_f].value);
				load_texture(text_FMS, xmbicons, 128);
				load_texture(text_FMS+(33*65536), blankBG, 320);
				load_texture(text_FMS+(37*65536), xmbdevs, 64);
		//		mip_texture( text_FMS+(31*65536), text_bmp, 320, 178, -2); // -> 216x183

				for(int n=0; n<MAX_XMB_TEXTS; n++)
				{
					xmb_txt_buf[n].used=0;
					xmb_txt_buf[n].data=text_bmpUPSR+(n*XMB_TEXT_WIDTH*XMB_TEXT_HEIGHT*4);
				}
				xmb_txt_buf_max=0;
				for(int n=0; n<xmb[1].size; n++) xmb[1].member[n].data=-1;

				draw_xmb_clock(xmb_clock, 1);
				draw_xmb_icon_text(xmb_icon);
				memset(text_bmp, 0, 8294400);
				load_texture(text_bmp, xmbbg, 1920);
			}
		}
		slide_xmb_right();

//		draw_xmb_bare(1, 1, 0, 1);
		free(pane);
}

int delete_game_cache()
{
		t_dir_pane_bare *pane =  (t_dir_pane_bare *) malloc(sizeof(t_dir_pane_bare)*MAX_PANE_SIZE_BARE);
		int max_dir=0;
		int ret_f=-1;
		slide_xmb_left(2);
;
		char string1[1024];
		ps3_home_scan_ext_bare(game_cache_dir, pane, &max_dir, (char*)"PS3NAME.DAT"); 
		if(max_dir==0) 
		{	
			abort_copy=0;
			//fix_perm_recursive(game_cache_dir);
			my_game_delete(game_cache_dir);
			mkdir(game_cache_dir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
			slide_xmb_right();
			return 0;
		}
		opt_list_max=0;
		char tmp_n[512];
		FILE *fpA;

		for(int n=0; n<max_dir; n++)
		{
			sprintf(string1, "%s/%s", pane[n].path, pane[n].name);
			if(stat(string1, &s3)>=0)
			{
				fpA = fopen ( string1, "r" );
				if(fpA!=NULL)
				{
					if(fgets( tmp_n, sizeof tmp_n, fpA ) != NULL)
					{
						tmp_n[32]=0x2e; tmp_n[33]=0x2e; tmp_n[34]=0x2e; tmp_n[35]=00;
						sprintf(opt_list[opt_list_max].label, "%s", tmp_n);
						sprintf(opt_list[opt_list_max].value, "%s", pane[n].path);
						opt_list_max++;
					}
					fclose(fpA);
					if(opt_list_max>=MAX_LIST_OPTIONS) break;
				}
			}
		}

		if(opt_list_max)
		{
			use_analog=1;
			float b_mX=mouseX;
			float b_mY=mouseY;
			mouseX=660.f/1920.f;
			mouseY=225.f/1080.f;
			is_any_xmb_column=xmb_icon;
			ret_f=open_list_menu((char*) "Delete Game Cache", 600, opt_list, opt_list_max, 660, 225, 16, 0);
			is_any_xmb_column=0;
			use_analog=0;
			mouseX=b_mX;
			mouseY=b_mY;
			if(ret_f!=-1) 
			{
				if(strstr(opt_list[ret_f].value, game_cache_dir)!=NULL)
				{
					abort_copy=0;
					//fix_perm_recursive(opt_list[ret_f].value);
					my_game_delete(opt_list[ret_f].value);
					rmdir(opt_list[ret_f].value);
				}
			}
		}

		slide_xmb_right();
		free(pane);
		return ret_f;
}

void update_fm_stripe()
{
	if(fm_sel==fm_sel_old) return;
	fm_sel_old=fm_sel;
	memcpy(text_FMS+737280, text_FMS, 368640);

	max_ttf_label=0;
	print_label_ex( 0.104f, 0.13f, 1.0f, (fm_sel&1  ? 0xffc0c0c0 : COL_FMFILE), (char*)"Games",  1.04f, 0.0f, 0, (fm_sel&1  ? 1.4f : 1.1f), 21.f, 1);
	print_label_ex( 0.230f, 0.13f, 1.0f, (fm_sel&2  ? 0xffc0c0c0 : COL_FMFILE), (char*)"Update", 1.04f, 0.0f, 0, (fm_sel&2  ? 1.4f : 1.1f), 21.f, 1);
	print_label_ex( 0.359f, 0.13f, 1.0f, (fm_sel&4  ? 0xffc0c0c0 : COL_FMFILE), (char*)"About",  1.04f, 0.0f, 0, (fm_sel&4  ? 1.4f : 1.1f), 21.f, 1);
	print_label_ex( 0.479f, 0.13f, 1.0f, (fm_sel&8  ? 0xffc0c0c0 : COL_FMFILE), (char*)"Help",   1.04f, 0.0f, 0, (fm_sel&8  ? 1.4f : 1.1f), 21.f, 1);
	print_label_ex( 0.609f, 0.13f, 1.0f, (fm_sel&16 ? 0xffc0c0c0 : COL_FMFILE), (char*)"Themes", 1.04f, 0.0f, 0, (fm_sel&16 ? 1.4f : 1.1f), 21.f, 1);
	flush_ttf(text_FMS+737280, 1920, 48);
}

void set_fm_stripes()
{
	fm_sel=0; fm_sel_old=15;
	load_texture(text_FMS, playBG, 1920); memcpy(text_FMS+368640, text_FMS, 368640); memcpy(text_FMS+737280, text_FMS, 368640);
	update_fm_stripe();
}

void draw_fileman()
{
		//if(c_opacity2<0x01) return;

		set_texture( text_OFF_2, 320, 320); 
		display_img(1775-(int)(1920.0f*new_offset), 24+(int)(1080.0f*new_offset), 48, 48, 96, 96, -0.2f, 320, 320);

		update_fm_stripe();

//		draw_square(-1.0f, (0.5f - 0.00f)*2.0f, 2.0f, 0.087f, 0.0f, 0x000000ff);
//		draw_square(-1.0f, (0.5f - 0.90f)*2.0f, 2.0f, 0.20f, 0.0f, 0x000000ff);

		set_texture( text_FMS+737280, 1920, 48); 
		display_img(0, 47, 1920, 60, 1920, 48, 0.0f, 1920, 48);
		set_texture( text_FMS, 1920, 48); 
		display_img(0, 952, 1920, 76, 1920, 48, 0.0f, 1920, 48);

		set_texture( text_bmpUPSR+V_WIDTH*4*(int)((107.f/1080.f)*V_HEIGHT), V_WIDTH, V_HEIGHT-(int)((235.f/1080.f)*V_HEIGHT));//V_HEIGHT-); 
		display_img_nr(0, (int)((107.f/1080.f)*V_HEIGHT), V_WIDTH, V_HEIGHT-(int)((235.f/1080.f)*V_HEIGHT), V_WIDTH, V_HEIGHT-(int)((235.f/1080.f)*V_HEIGHT), 0.0f, V_WIDTH, V_HEIGHT-(int)((235.f/1080.f)*V_HEIGHT));

}

int context_menu(char *_capt, int _type, char *c_pane, char *o_pane)
{
			char _cap[512];
			sprintf(_cap, "%s", _capt); _cap[32]='.';_cap[33]='.';_cap[34]='.';_cap[35]=0;
			opt_list_max=0;
			u8 multiple_entries=0;
			if(!strcmp(_cap, "..")) goto skip_dd;
			if(!strcmp(_cap, "Multiple folders") || !strcmp(_cap, "Multiple files")) multiple_entries=1;

			if(strstr(o_pane, "/dev_bdvd")==NULL && strstr(o_pane, "/pvd_usb")==NULL && strstr(o_pane, "/app_home")==NULL && strlen(o_pane)>1 && strstr(_cap, "net_host")==NULL && strcmp(c_pane, o_pane))
			{
				if( (strstr(c_pane, "/dev_bdvd")==NULL && strcmp(_cap, "dev_bdvd")) || (strstr(c_pane, "/dev_bdvd")!=NULL && stat((char*)"/dev_bdvd/PS3_GAME", &s3)>=0))
				{
					sprintf(opt_list[opt_list_max].label, "%s", "Copy");
					sprintf(opt_list[opt_list_max].value, "%s", "copy"); opt_list_max++;
				}

				if(strstr(c_pane, "/dev_bdvd")==NULL && strstr(c_pane, "/pvd_usb")==NULL && strstr(c_pane, "/app_home")==NULL && strstr(c_pane, "/ps3_home")==NULL && strlen(c_pane)>1  && strlen(o_pane)>1 
					&& !(!strcmp(c_pane, "/dev_hdd0") && (!strcmp(_cap, "game") || !strcmp(_cap, "vsh") || !strcmp(_cap, "home") || !strcmp(_cap, "mms") || !strcmp(_cap, "vm") || !strcmp(_cap, "etc") || !strcmp(_cap, "drm"))) )
				{
					sprintf(opt_list[opt_list_max].label, "%s", "Move");
					sprintf(opt_list[opt_list_max].value, "%s", "move"); opt_list_max++;
				}
			}

			if(strstr(c_pane, "/dev_bdvd")==NULL && strstr(c_pane, "/pvd_usb")==NULL && strstr(c_pane, "/app_home")==NULL && strstr(c_pane, "/ps3_home")==NULL && strlen(c_pane)>1
				&& !(!strcmp(c_pane, "/dev_hdd0") && (!strcmp(_cap, "game") || !strcmp(_cap, "vsh") || !strcmp(_cap, "home") || !strcmp(_cap, "mms") || !strcmp(_cap, "vm") || !strcmp(_cap, "etc") || !strcmp(_cap, "drm"))))
			{
				if(strstr(c_pane, "/net_host")==NULL && !multiple_entries)
				{
					sprintf(opt_list[opt_list_max].label, "%s", "Rename");
					sprintf(opt_list[opt_list_max].value, "%s", "rename"); opt_list_max++;
				}

				if(strstr(c_pane, "/net_host")==NULL || (strstr(c_pane, "/net_host")!=NULL && _type==1))
				{
					sprintf(opt_list[opt_list_max].label, "%s", "Delete");
					sprintf(opt_list[opt_list_max].value, "%s", "delete"); opt_list_max++;
				}
			}

			if(strcmp(c_pane, o_pane) && strstr(c_pane, "/dev_hdd0")!=NULL && strstr(o_pane, "/dev_hdd0")!=NULL && !multiple_entries)
			{
				sprintf(opt_list[opt_list_max].label, "%s", "Create Shortcut");
				sprintf(opt_list[opt_list_max].value, "%s", "shortcut"); opt_list_max++;
			}

			if(!strcmp(_cap, "PS3_GAME") && !multiple_entries)
			{
				if(strstr(c_pane, "/dev_hdd0")!=NULL)
				{
					sprintf(opt_list[opt_list_max].label, "%s", "Shadow for PKG game");
					sprintf(opt_list[opt_list_max].value, "%s", "pkgshortcut"); opt_list_max++;
				}
				if(strstr(c_pane, "/dev_usb")!=NULL)
				{
					sprintf(opt_list[opt_list_max].label, "%s", "Activate BD-Mirror");
					sprintf(opt_list[opt_list_max].value, "%s", "bdmirror"); opt_list_max++;
				}
			}

			if(strstr(c_pane, "/net_host")!=NULL || strstr(_cap, "net_host")!=NULL)
			{
				sprintf(opt_list[opt_list_max].label, "%s", "Refresh Net Host");
				sprintf(opt_list[opt_list_max].value, "%s", "nethost"); opt_list_max++;
			}

			if(_type==1 && !multiple_entries)
			{
				sprintf(opt_list[opt_list_max].label, "%s", "Open in HEX Viewer");
				sprintf(opt_list[opt_list_max].value, "%s", "view"); opt_list_max++;
			}

			if(_type==0 && strstr(c_pane, "/ps3_home")==NULL && strstr(c_pane, "/net_host")==NULL && !multiple_entries)
			{
				sprintf(opt_list[opt_list_max].label, "%s", "Properties");
				sprintf(opt_list[opt_list_max].value, "%s", "test"); opt_list_max++;
			}

skip_dd:

			if(strstr(c_pane, "/dev_bdvd")==NULL && strstr(c_pane, "/pvd_usb")==NULL && strstr(c_pane, "/app_home")==NULL && strstr(c_pane, "/ps3_home")==NULL && strlen(c_pane)>1)
			{
				sprintf(opt_list[opt_list_max].label, "%s", "Create New Folder");
				sprintf(opt_list[opt_list_max].value, "%s", "newfolder"); opt_list_max++;
			}

			if(opt_list_max)
			{
				use_analog=0;
				use_depth=0;
				float b_mX=mouseX;
				float b_mY=mouseY;
				if(mouseX>0.84f) mouseX=0.84f;
				if(mouseY>0.60f) mouseY=0.60f;
				int ret_f=open_dd_menu( _cap, 300, opt_list, opt_list_max, 660, 225, 16);
				use_depth=1;
				use_analog=0;
				mouseX=b_mX;
				mouseY=b_mY;
				return ret_f;
			}

//just_leave_dd:
	new_pad=0;
	return -1;
}

void apply_theme (const char *theme_file, const char *theme_path)
{
	char theme_name[1024];
	sprintf(theme_name, "%s", theme_file); theme_name[strlen(theme_name)-4]=0;
	char *pch=theme_name;
	char *pathpos=strrchr(pch,'/');
	char temp_text[512];
	char filename[1024];

	sprintf(temp_text, "Applying \x22%s\x22 theme, please wait...", pathpos+(pathpos[1]=='_' ? 2 : 1));

				dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, temp_text, dialog_fun2, (void*)0x0000aaab, NULL );
				flipc(60);

				char th_file[32], th2_file[64];

				sprintf(th_file, "%s", "AVCHD.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, avchdBG, 0);

				sprintf(th_file, "%s", "PICBG.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, userBG, 0);

				sprintf(th_file, "%s", "ICON0.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, blankBG, 0);

				sprintf(th_file, "%s", "PICPA.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) 
				{ 
					file_copy(filename, playBGR, 0);
					//sprintf(filename,  "/dev_hdd0/game/%s/PIC1.PNG", app_path);
					//file_copy(playBGR, filename, 0);
				}

				sprintf(th_file, "%s", "XMB0.PNG"); sprintf(th2_file, "%s/%s", theme_path, th_file);
				sprintf(filename,  "/dev_hdd0/game/%s/PIC0.PNG", app_path);
				if(stat(th2_file, &s3)>=0) {
					sprintf(filename,  "/dev_hdd0/game/%s/PIC0.PNG", app_path);
					file_copy(th2_file, filename, 0);
				} else remove(filename);

				sprintf(th_file, "%s", "XMB1.PNG"); sprintf(th2_file, "%s/%s", theme_path, th_file);
				if(stat(th2_file, &s3)>=0) {
					sprintf(filename,  "/dev_hdd0/game/%s/PIC1.PNG", app_path);
					file_copy(th2_file, filename, 0);
				}

				//sprintf(th_file, "%s", "XMB2.PNG"); sprintf(th2_file, "%s/%s", theme_path, th_file);
				//sprintf(filename,  "/dev_hdd0/game/%s/PIC2.PNG", app_path);
				//if(stat(th2_file, &s3)>=0) {
				//	file_copy(th2_file, filename, 0);
				//} else remove(filename);

				sprintf(th_file, "%s", "SND0.AT3"); sprintf(th2_file, "%s/%s", theme_path, th_file);
				sprintf(filename,  "/dev_hdd0/game/%s/SND0.AT3", app_path);
				if(stat(th2_file, &s3)>=0) {
					file_copy(th2_file, filename, 0);
				} else remove(filename);

				sprintf(th_file, "%s", "FMS.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, playBG, 0);

				sprintf(th_file, "%s", "HDD.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, iconHDD, 0);
				sprintf(th_file, "%s", "USB.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, iconUSB, 0);
				sprintf(th_file, "%s", "BLU.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, iconBLU, 0);
				sprintf(th_file, "%s", "NET.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, iconNET, 0);
				sprintf(th_file, "%s", "OFF.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, iconOFF, 0);
				sprintf(th_file, "%s", "CFC.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, iconCFC, 0);
				sprintf(th_file, "%s", "SDC.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, iconSDC, 0);
				sprintf(th_file, "%s", "MSC.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);
				if(stat(filename, &s3)>=0) file_copy(filename, iconMSC, 0);
				int flipF;
				for(flipF = 0; flipF<9; flipF++){
					sprintf(th_file, "AUR%i.JPG", flipF); sprintf(filename, "%s/%s", theme_path, th_file);
					if(stat(filename, &s3)>=0) {sprintf(th2_file, "%s/%s", app_usrdir, th_file); file_copy(filename, th2_file, 0);}
				}

				for(flipF = 0; flipF<9; flipF++){
					sprintf(th_file, "font%i.ttf", flipF); sprintf(filename, "%s/%s", theme_path, th_file);
					if(stat(filename, &s3)>=0) {sprintf(th2_file, "%s/fonts/user/%s", app_usrdir, th_file); file_copy(filename, th2_file, 0);}
				}

				sprintf(th_file, "%s", "BOOT.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "LEGEND2.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }

				sprintf(th_file, "%s", "XMB.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "XMB64.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "XMB2.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "XMBBG.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "PRB.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "GLO.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); remove(th2_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "GLC.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); remove(th2_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "GLC2.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); remove(th2_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "GLC3.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); remove(th2_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }

				sprintf(th_file, "%s", "NOID.JPG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "DOX.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				load_texture(text_DOX, th2_file, dox_width);

				sprintf(th_file, "%s", "SBOX.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);		sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "CBOX.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);		sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "CBOX2.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); remove(th2_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "CBOX3.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); remove(th2_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "CBOX4.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); remove(th2_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "GBOX.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);		sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "GBOX2.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); remove(th2_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }

				sprintf(th_file, "%s", "MP_HR.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				sprintf(th_file, "%s", "MP_LR.PNG"); sprintf(filename, "%s/%s", theme_path, th_file);	sprintf(th2_file, "%s/%s", app_usrdir, th_file); if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }

				if(V_WIDTH>1280)
					sprintf(filename, "%s/MP_HR.PNG",app_usrdir);
				else
				{
					sprintf(filename, "%s/MP_LR.PNG",app_usrdir);
					mp_WIDTH=15, mp_HEIGHT=21; //mouse icon LR
				}

				if(stat(filename, &s3)>=0) {load_texture((unsigned char *) mouse, filename, mp_WIDTH);}

				sprintf(th_file, "%s", "SOUND.BIN"); sprintf(filename, "%s/%s", theme_path, th_file);
				sprintf(th2_file, "%s/%s", app_usrdir, th_file);
				remove(th2_file);
				if(stat(filename, &s3)>=0) { file_copy(filename, th2_file, 0); }
				if((stat(th2_file, &s3)>=0 && !(current_mp3!=0 && max_mp3!=0)) && theme_sound)
					main_mp3((char*)th2_file); 
				else 
					{
						if(!(current_mp3!=0 && max_mp3!=0))
						main_mp3((char*)"SILENCE.BIN");
					}

				sprintf(th_file, "%s", "COLOR.INI"); sprintf(filename, "%s/%s", theme_path, th_file);
				sprintf(th2_file, "%s/%s", app_usrdir, th_file); remove(th2_file); 
				if(stat(filename, &s3)>=0) file_copy(filename, th2_file, 0); 
				else {
					sprintf(filename, "%s/COLOR.BIN", app_usrdir);
					sprintf(th2_file, "%s/COLOR.INI", app_usrdir);
					if(stat(filename, &s3)>=0) file_copy(filename, th2_file, 0) ;
				}


	load_texture(text_bmpIC, blankBG, 320);
	if(cover_mode!=8)
	{
		load_texture(text_bmpUBG, avchdBG, 1920);
		load_texture(text_bmpUPSR, playBGR, 1920);
		set_fm_stripes();
	}
		load_texture(text_HDD, iconHDD, 320);
		load_texture(text_USB, iconUSB, 320);
		load_texture(text_BLU_1, iconBLU, 320);
		load_texture(text_NET_6, iconNET, 320);

		load_texture(text_OFF_2, iconOFF, 320);
		load_texture(text_CFC_3, iconCFC, 320);
		load_texture(text_SDC_4, iconSDC, 320);
		load_texture(text_MSC_5, iconMSC, 320);
	parse_color_ini();

				cellMsgDialogAbort();
				state_read=1;
				state_draw=1;
}

void draw_xmb_title(u8 *buffer, xmbmem *member, int cn, u32 col1, u32 col2, u8 _xmb_col)
{
/*		if(c_opacity2<0x21) c_opacity2=0x21;
		if(c_opacity2>=0x20)
		{
			col1&=(0x00ffffff)|(c_opacity2<<24);
			col2&=(0x00ffffff)|(c_opacity2<<24);
		}*/
//		print_label_ex( 0.0f, 0.0f, 0.8f, 0xff101010, str1, 1.03f, 0.0f, 0, 3.0f, 24.0f, 0);
//		print_label_ex( 0.0f, 0.62f, 0.5f, 0xff101010, str2, 1.00f, 0.0f, 0, 3.0f, 24.0f, 0);
		memset(buffer, 0, XMB_TEXT_WIDTH*XMB_TEXT_HEIGHT*4); //flush_ttf(buffer, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT);
		//for(int fsr=0; fsr<XMB_TEXT_WIDTH*XMB_TEXT_HEIGHT*4; fsr+=4) *(uint32_t*) ((uint8_t*)(buffer)+fsr)=0x10101001;

//		blur_texture(buffer, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT, 0, 0, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT,  0, 0, 2, 2);
		if((_xmb_col==6 || _xmb_col==7) && xmb_game_bg)
		{
//			print_label_ex( 0.004f, 0.04f, 0.75f, 0x101010ff, member[cn].name, 1.04f, 0.0f, 0, 3.0f, 22.0f, 0);
//			print_label_ex( 0.003f, 0.00f, 0.75f, 0x101010ff, member[cn].name, 1.04f, 0.0f, 0, 3.0f, 22.0f, 0);
//			print_label_ex( 0.003f, 0.02f, 0.75f, 0x101010ff, member[cn].name, 1.04f, 0.0f, 0, 3.0f, 22.0f, 0);
//			print_label_ex( 0.007f, 0.02f, 0.75f, 0x101010ff, member[cn].name, 1.04f, 0.0f, 0, 3.0f, 22.0f, 0);
			print_label_ex( 0.005f, 0.03f, 0.75f, 0x80101010, member[cn].name, 1.04f, 0.0f, 0, 3.0f, 22.0f, 0);
		}

		if(_xmb_col==2 && member[cn].option_size) //settings
			print_label_ex( 0.93f, 0.2f, 0.45f, col1, member[cn].option[member[cn].option_selected].label, 1.04f, 0.0f, 0, 3.0f, 22.0f, 2);

		if(!((_xmb_col>3 && _xmb_col<8 && member[cn].type<6) || (_xmb_col==8 && member[cn].type>7)))
			print_label_ex( 0.0f, 0.57f, 0.5f, col2, member[cn].subname, 1.02f, 0.0f, 0, 3.0f, 23.0f, 0);

		flush_ttf(buffer, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT);
//		if((_xmb_col==6 || _xmb_col==7) && xmb_game_bg)
//		{
//			blur_texture(buffer, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT, 0, 0, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT-32,  0, 0, 1, 1);
//			blur_texture(buffer, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT, 0, 0, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT-32,  0, 0, 1, 2);
//		}
		print_label_ex( 0.004f, 0.02f, 0.75f, col1, member[cn].name, 1.04f, 0.0f, 0, 3.0f, 22.0f, 0);
		flush_ttf(buffer, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT);

		u8 *xmb_dev_icon1=xmb_icon_dev;
		u8 *xmb_dev_icon2=xmb_icon_dev;
		if( (_xmb_col>3 && _xmb_col<8 && member[cn].type<6) || (_xmb_col==8 && member[cn].type>7 && member[cn].type<13))
		{
			if(_xmb_col==6 || _xmb_col==7) xmb_dev_icon1=xmb_icon_dev;
			else if(_xmb_col==5) xmb_dev_icon1=xmb_icon_dev+(1*8192);
			else if(_xmb_col==4) xmb_dev_icon1=xmb_icon_dev+(2*8192);
			else if(_xmb_col==8) xmb_dev_icon1=xmb_icon_dev+(16*8192);//retro

			if(_xmb_col==8)
			{
				xmb_dev_icon2=xmb_icon_dev+((17+(member[cn].type-8))*8192);
			}
			else
			{
				if(strstr(member[cn].file_path, "/dev_hdd")!=NULL) xmb_dev_icon2=xmb_icon_dev+(4*8192);
				else if(strstr(member[cn].file_path, "/dev_usb")!=NULL) xmb_dev_icon2=xmb_icon_dev+(5*8192);
				else if(strstr(member[cn].file_path, "/dev_bdvd")!=NULL) xmb_dev_icon2=xmb_icon_dev+(6*8192);
				else if(strstr(member[cn].file_path, "/pvd_usb")!=NULL) xmb_dev_icon2=xmb_icon_dev+(7*8192);
				else if(strstr(member[cn].file_path, "/dev_sd")!=NULL) xmb_dev_icon2=xmb_icon_dev+(13*8192);
				else if(strstr(member[cn].file_path, "/dev_ms")!=NULL) xmb_dev_icon2=xmb_icon_dev+(14*8192);
				else if(strstr(member[cn].file_path, "/dev_cf")!=NULL) xmb_dev_icon2=xmb_icon_dev+(15*8192);
			}
			int nip=0;
			put_texture_with_alpha_gen( buffer, xmb_dev_icon1, 64, 32, 64, XMB_TEXT_WIDTH, nip, XMB_TEXT_HEIGHT-32); nip+=64;
			if(_xmb_col==5)
			{
				if(!strcmp(member[cn].subname, "AVCHD")) 
					{ put_texture_with_alpha_gen( buffer, xmb_icon_dev+(10*8192), 64, 32, 64, XMB_TEXT_WIDTH, nip, XMB_TEXT_HEIGHT-32); nip+=64; }
				else if(!strcmp(member[cn].subname, "BDMV")) 
					{ put_texture_with_alpha_gen( buffer, xmb_icon_dev+(11*8192), 64, 32, 64, XMB_TEXT_WIDTH, nip, XMB_TEXT_HEIGHT-32); nip+=64; }
				else if(!strcmp(member[cn].subname, "DVD")) 
					{ put_texture_with_alpha_gen( buffer, xmb_icon_dev+(12*8192), 64, 32, 64, XMB_TEXT_WIDTH, nip, XMB_TEXT_HEIGHT-32); nip+=64; }
			}
			put_texture_with_alpha_gen( buffer, xmb_dev_icon2, 64, 32, 64, XMB_TEXT_WIDTH, nip, XMB_TEXT_HEIGHT-32); nip+=64;
			if(_xmb_col==6 && member[cn].game_id>=0 && member[cn].game_id<max_menu_list)
			{
				if(member[cn].game_user_flags & IS_FAV)
				{put_texture_with_alpha_gen( buffer, xmb_icon_dev+(8*8192), 64, 32, 64, XMB_TEXT_WIDTH, nip, XMB_TEXT_HEIGHT-32); nip+=64;}
			}
			if((_xmb_col==6 || _xmb_col==7) && member[cn].game_id>=0 && member[cn].game_id<max_menu_list)
			{
				if(member[cn].game_split==1)
				{put_texture_with_alpha_gen( buffer, xmb_icon_dev+(9*8192), 64, 32, 64, XMB_TEXT_WIDTH, nip, XMB_TEXT_HEIGHT-32); nip+=64;}
			}

		}

}

void add_xmb_member(xmbmem *_member, u16 *_size, char *_name, char *_subname,
		/*type*/u8 _type, /*status*/u8 _status, /*game_id*/int _game_id, /*icon*/u8 *_data, u16 _iconw, u16 _iconh, /*f_path*/char *_file_path, /*i_path*/ char *_icon_path, int _u_flags, int _split)
{
	if((*_size)>=MAX_XMB_MEMBERS-1) return;

	u16 size=(*_size);
	_member[size].is_checked = true;
	_member[size].type		=_type;
	_member[size].status	=_status;
	_member[size].game_id	=_game_id;

	_member[size].game_user_flags =_u_flags;
	_member[size].game_split	  =_split;

	_member[size].name[128]=0;
	sprintf(_member[size].name, "%s", _name);
	_member[size].subname[96]=0;
	sprintf(_member[size].subname, "%s", _subname);

	_member[size].option_size=0;
	_member[size].option_selected=0;

	_member[size].data=-1;
	_member[size].icon  =_data;
	_member[size].icon_buf = -1;
	_member[size].iconw =_iconw;
	_member[size].iconh =_iconh;

	sprintf(_member[size].file_path, "%s", _file_path);
	sprintf(_member[size].icon_path, "%s", _icon_path);

	(*_size)++;
}

void add_xmb_suboption(xmbopt *_option, u8 *_size, u8 _type, char *_label, char *_value)
{
	(void) _type;
	if((*_size)>=MAX_XMB_OPTIONS) return;
	u8 size=(*_size);
	//_option[size].type	= _type;
	sprintf(_option[size].label, "%s", _label);
	sprintf(_option[size].value, "%s", _value);
	(*_size)++;
}

void add_xmb_option(xmbmem *_member, u16 *_size, char *_name, char *_subname, char *_optionini)
{
	if((*_size)>=MAX_XMB_MEMBERS-1) return;

	u16 size=(*_size);
	_member[size].type		=  7;//option
	_member[size].status	=  2;//loaded
	_member[size].game_id	= -1;

	_member[size].game_user_flags = 0;
	_member[size].game_split	  = 0;

	sprintf(_member[size].name, "%s", _name);
	sprintf(_member[size].subname, "%s", _subname);

	_member[size].option_size=0;
	_member[size].option_selected=0;
	sprintf(_member[size].optionini, "%s", _optionini);

	_member[size].data=-1;
	_member[size].icon  = xmb_icon_tool;
	_member[size].icon_buf = -1;
	_member[size].iconw = 128;
	_member[size].iconh = 128;

	sprintf(_member[size].file_path, "%s", (char*)"/");
	sprintf(_member[size].icon_path, "%s", (char*)"/");

	(*_size)++;
}

void reset_xmb_checked()
{
	for(int n=0;n<MAX_XMB_ICONS;n++)
		for(int m=0;m<xmb[n].size;m++)
			xmb[n].member[m].is_checked=false;
}

void free_all_buffers()
{
	while(is_decoding_jpg || is_decoding_png){ sys_timer_usleep(3336); cellSysutilCheckCallback();}
	int n;
	for(n=0; n<MAX_XMB_THUMBS; n++) xmb_icon_buf[n].used=-1;

	for(n=((xmb[6].member[1].icon==xmb_icon_blu) ? 2 : 1); n<xmb[6].size; n++) 
	{
		xmb[6].member[n].icon_buf=-1;
		xmb[6].member[n].status=0;
	}

	for(n=0; n<xmb[7].size; n++) 
	{
		xmb[7].member[n].icon_buf=-1;
		xmb[7].member[n].status=0;
	}

	for(n=1; n<xmb[8].size; n++) 
		//if(xmb[8].member[n].icon!=xmb_icon_retro)//xmb[8].member[n].icon!=xmb[0].data && 
		{
			xmb[8].member[n].icon_buf=-1;
			xmb[8].member[n].status=0;
			xmb[8].member[n].icon=xmb_icon_retro;
		}

	for(n=0; n<xmb[5].size; n++) 
	{
		if(xmb[5].member[n].icon!=xmb_icon_film && xmb[5].member[n].icon!=xmb_icon_showtime)
		{
			xmb[5].member[n].icon_buf=-1;
			xmb[5].member[n].status=0;
			xmb[5].member[n].icon=xmb_icon_film;
		}
	}

	for(n=0; n<xmb[3].size; n++) 
	{
		if(xmb[3].member[n].icon!=xmb_icon_photo)
		{
			xmb[3].member[n].icon_buf=-1;
			xmb[3].member[n].status=0;
			xmb[3].member[n].icon=xmb_icon_photo;
		}
	}
}

void reset_xmb(u8 _flag)
{
	for(int n=0; n<MAX_XMB_ICONS; n++)
	{
		if(_flag && n!=8) //skip retro
		{
			xmb[n].size=0;
			xmb[n].first=0;
			xmb[n].init=0;
		}
		xmb[n].data=text_FMS+(n*65536);
	}
	xmb[8].data=xmb_icon_retro;
	xmb[9].data=text_FMS+(8*65536);
}

int find_free_buffer(const int _col)
{
	(void) _col;
	int n;
	for(n=0; n<MAX_XMB_THUMBS; n++)
	{
		if(xmb_icon_buf[n].used==-1) return n;
	}

	if(xmb_icon==3)
	{
		for(n=0; n<xmb[3].size; n++) 
		{
			if(xmb[3].member[n].icon!=xmb_icon_photo && (n<(xmb[3].first-3) || n>(xmb[3].first+7)))
			{
				xmb[3].member[n].icon_buf=-1;
				xmb[3].member[n].status=0;
				xmb[3].member[n].icon=xmb_icon_photo;
			}
		}
	 	for(n=0; n<MAX_XMB_THUMBS; n++) xmb_icon_buf[n].used=-1;
		return 0;
	}

	else
		free_all_buffers();

	return 0;

/*
	int b;
	for(b=_cn+12; b<xmb[_col].size; b++) {
		if(b>=xmb[_col].size) break;
		if(xmb[_col].member[b].icon_buf>=0) {
			xmb[_col].member[b].status=0; return xmb[_col].member[b].icon_buf;
		}
	}
	for(b=_cn-12; b>0; b--) {
		if(b<0) break;
		if(xmb[_col].member[b].icon_buf>=0) 
		{
			xmb[_col].member[b].status=0; return xmb[_col].member[b].icon_buf;
		}
	}
*/
//	sprintf(string1, "Buffer: %i, Title: %i", xmb[_col].member[b].icon_buf, b);dialog_ret=0;		cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
//	return -1;
}

void draw_xmb_icons(xmb_def *_xmb, const int _xmb_icon_, int _xmb_x_offset, int _xmb_y_offset, const bool _recursive, int sub_level)
{
	int _xmb_icon = _xmb_icon_;

	int first_xmb=_xmb_icon-2;
	int xpos, _xpos;
	u8 subicons = (sub_level!=-1);
	if(sub_level<0) sub_level=0;
	_xpos=-90+_xmb_x_offset - (200*sub_level);
	int ypos=0, tw=0, th=0;
	u16 icon_x=0;
	u16 icon_y=0;
	int mo_of=0;
	float mo_of2=0.0f;
	bool one_done=false;

	if(_xmb_icon>3 && _xmb_x_offset>0) {first_xmb--; _xpos-=200;}
	for(int n=first_xmb; n<MAX_XMB_ICONS; n++)
	{
		_xpos+=200;
		xpos = _xpos;

		_xmb_icon = _xmb_icon_;
		if(_xmb_x_offset>=100 && _xmb_icon>1 && !subicons) {_xmb_icon--; }
		if(_xmb_x_offset<=-100 && _xmb_icon<MAX_XMB_ICONS-1 && !subicons) {_xmb_icon++;}

		if(n<1) continue;
		if(sub_level && n!=xmb_icon) continue;
		set_texture(_xmb[n].data, 128, 128); //icon
		mo_of=abs((int)(_xmb_x_offset*0.18f));
		if(_xmb[_xmb_icon].first>=_xmb[_xmb_icon].size) _xmb[_xmb_icon].first=0;
		if(n==xmb_icon)
		{
			if(egg)
				display_img_angle(xpos-(36-mo_of)/2, 230-(36-mo_of), 164-mo_of, 164-mo_of, 128, 128, 0.8f, 128, 128, angle);
			else
				display_img(xpos-(36-mo_of)/2, 230-(36-mo_of), 164-mo_of, 164-mo_of, 128, 128, 0.8f, 128, 128);
			set_texture(xmb_col, 300, 30); //column name
			display_img(xpos-86, 340, 300, 30, 300, 30, 0.7f, 300, 30);

			if(_xmb[_xmb_icon].size>0 && subicons && !(key_repeat && ( (old_pad & BUTTON_LEFT) || (old_pad & BUTTON_RIGHT)) && (xmb_icon!=1 && xmb_icon!=MAX_XMB_ICONS-1)) && (abs(_xmb_x_offset)<100 || _xmb_icon != _xmb_icon_))
			{
				xpos = _xpos;
				if(_xmb_x_offset>=100 && !subicons) xpos-=200;
				if(_xmb_x_offset<=-100 && !subicons) xpos+=200;

				int cn;
				int cn3=1;
				int first_xmb_mem = _xmb[_xmb_icon].first;
				int cnmax=3;
				if(_xmb[_xmb_icon].first>2 && _xmb_y_offset>0) {first_xmb_mem--; cn3--;}
				for(int m=0;m<4;m++) // make it pleasureble to watch while loading column
				{
					if(m==1)
					{
						cn3=0;
						first_xmb_mem = _xmb[_xmb_icon].first-1;
						cnmax=1;
					}

					if(m==2)
					{
						cn3=-1;
						first_xmb_mem = _xmb[_xmb_icon].first-2;
						cnmax=0;
					}

					if(m==3)
					{
						cn3=3;
						first_xmb_mem = _xmb[_xmb_icon].first+2;
						cnmax=8;
					}

					if(_xmb[_xmb_icon].first>2 && _xmb_y_offset>0) {first_xmb_mem--; cn3--;}


					for(cn=first_xmb_mem; (cn<_xmb[_xmb_icon].size && cn3<cnmax); cn++)
					{
						if(!_xmb[_xmb_icon].member[cn].is_checked)
						{
							if( ( (_xmb_icon>2 && _xmb_icon<6) || _xmb_icon==8) 
								&& (_xmb[_xmb_icon].member[cn].type>7 || (_xmb[_xmb_icon].member[cn].type>1 && _xmb[_xmb_icon].member[cn].type<6) )
								&& (stat(_xmb[_xmb_icon].member[cn].file_path, &s3)<0)
							)

							{
								delete_xmb_member(_xmb[_xmb_icon].member, &_xmb[_xmb_icon].size, cn);
								if(cn>=_xmb[_xmb_icon].size) break;
								sort_xmb_col(_xmb[_xmb_icon].member, _xmb[_xmb_icon].size, cn);
							}
							else
								_xmb[_xmb_icon].member[cn].is_checked=true;
						}
						cn3++;
						if(cn<0) continue;
						if(sub_level && cn3!=2) continue;

						tw=_xmb[_xmb_icon].member[cn].iconw; th=_xmb[_xmb_icon].member[cn].iconh;
						if(tw>320 || th>176)
						{
							if(tw>th) {th= (int)((float)th/((float)tw/320.f)); tw=320;}
							else {tw= (int)((float)tw/((float)th/176.f)); th=176;}
							if(tw>320) {th= (int)((float)th/((float)tw/320.f));	tw=320;}
							if(th>176) {tw= (int)((float)tw/((float)th/176.f));	th=176;}
						}

						if(cn3!=2) {tw/=2; th/=2;}
						mo_of2=2.f-(abs(_xmb_y_offset)/90.0f);

						if( (_xmb_y_offset!=0) )
						{
							if( (_xmb_y_offset>0 && cn3==1) || (_xmb_y_offset<0 && cn3==3) )
							{
								tw=_xmb[_xmb_icon].member[cn].iconw; th=_xmb[_xmb_icon].member[cn].iconh;
								if(tw>320 || th>176)
								{
									if(tw>th) {th= (int)((float)th/((float)tw/320.f)); tw=320;}
									else {tw= (int)((float)tw/((float)th/176.f)); th=176;}
									if(tw>320) {th= (int)((float)th/((float)tw/320.f));	tw=320;}
									if(th>176) {tw= (int)((float)tw/((float)th/176.f));	th=176;}
								}
								tw=(int)(tw/mo_of2); th=(int)(th/mo_of2);
							}
							else if( (_xmb_y_offset!=0 && cn3==2))
							{
								tw=_xmb[_xmb_icon].member[cn].iconw; th=_xmb[_xmb_icon].member[cn].iconh;
								if(tw>320 || th>176)
								{
									if(tw>th) {th= (int)((float)th/((float)tw/320.f)); tw=320;}
									else {tw= (int)((float)tw/((float)th/176.f)); th=176;}
									if(tw>320) {th= (int)((float)th/((float)tw/320.f));	tw=320;}
									if(th>176) {tw= (int)((float)tw/((float)th/176.f));	th=176;}
								}
								tw=(int)(tw/(3.f-mo_of2)); th=(int)(th/(3.f-mo_of2));
							}
						}

						if(cn3<1) ypos=cn3*90+_xmb_y_offset;
						else if(cn3==1) ypos=cn3*90 + ( (_xmb_y_offset>0) ? (int)(_xmb_y_offset*3.566f) : (_xmb_y_offset) );
						else if(cn3==2) {ypos = 411 + ( (_xmb_y_offset>0) ? (int)(_xmb_y_offset*2.377f) : (int)(_xmb_y_offset*3.566f) );}
						else if(cn3==3) ypos=(cn3-3)*90 + 625 + ( (_xmb_y_offset>0) ? _xmb_y_offset : (int)(_xmb_y_offset*2.377f) );
						else if(cn3 >3) ypos=(cn3-3)*90 + 625 + _xmb_y_offset;

						if((_xmb[_xmb_icon].member[cn].status==1 || _xmb[_xmb_icon].member[cn].status==0) && !_recursive && !key_repeat)// || (c_opacity_delta!=0 && dim==1 && c_opacity2>0x30 && c_opacity2<0x42))
						{
							if(_xmb[_xmb_icon].member[cn].status==0)
							{
								_xmb[_xmb_icon].member[cn].status=1;
								xmb_icon_buf_max=find_free_buffer(_xmb_icon);
								xmb_icon_buf[xmb_icon_buf_max].used=cn;
								xmb_icon_buf[xmb_icon_buf_max].column=_xmb_icon;

								_xmb[_xmb_icon].member[cn].icon = xmb_icon_buf[xmb_icon_buf_max].data;
								_xmb[_xmb_icon].member[cn].icon_buf=xmb_icon_buf_max;
							}

	//						load_png_partial( _xmb[_xmb_icon].member[cn].icon, _xmb[_xmb_icon].member[cn].icon_path, _xmb[_xmb_icon].member[cn].iconw, _xmb[_xmb_icon].member[cn].iconh/2, 0);
							if(_xmb_icon==5 || _xmb_icon==3 || _xmb_icon==8) 
							{
								if(_xmb_icon==8 && (strstr(_xmb[_xmb_icon].member[cn].icon_path,".png")!=NULL || strstr(_xmb[_xmb_icon].member[cn].icon_path,".PNG")!=NULL)) 
									load_png_threaded( _xmb_icon, cn);
								else
									load_jpg_threaded( _xmb_icon, cn);
							}
							else
							{
								load_png_threaded( _xmb_icon, cn);
							}
						}
						if(_xmb[_xmb_icon].member[cn].status==1 || (_xmb[_xmb_icon].member[cn].status==0 && (_recursive || key_repeat)))
						{
							tw=128; th=128;
							if(cn3!=2) {tw/=2; th/=2;}
							icon_x=xpos+64-tw/2;
							icon_y=ypos;

							set_texture(_xmb[0].data, 128, 128); //icon
							display_img_angle(icon_x, icon_y, tw, th, 128, 128, 0.5f, 128, 128, angle);

						}

						if(_xmb[_xmb_icon].member[cn].status==2)
						{
							//if(cn3!=2) {tw/=2; th/=2;}
							icon_x=xpos+64-tw/2;
							icon_y=ypos;

							set_texture(_xmb[_xmb_icon].member[cn].icon, _xmb[_xmb_icon].member[cn].iconw, _xmb[_xmb_icon].member[cn].iconh); //icon

							if((is_video_loading || is_music_loading || is_photo_loading || is_retro_loading || is_game_loading || is_any_xmb_column) 
								&& ( (_xmb_icon==1 && cn==2) || (_xmb_icon==6 && cn==0) || (_xmb_icon==8 && cn==0))
								)
							display_img_angle(icon_x, icon_y,
								tw, 
								th, 
								tw,
								th,
								0.5f, 
								tw,
								th, angle); 
							else

							display_img(icon_x, icon_y,
								tw, 
								th, 
								tw, //_xmb[_xmb_icon].member[cn].iconw, 
								th, //_xmb[_xmb_icon].member[cn].iconh, 
								0.5f, 
								tw, //_xmb[_xmb_icon].member[cn].iconw, 
								th); //_xmb[_xmb_icon].member[cn].iconh);

						}


						if(_xmb[_xmb_icon].member[cn].data==-1 && _xmb_x_offset==0 && !one_done/*|| (_xmb_icon != _xmb_icon_ && abs(_xmb_x_offset)==150)*/)// || (c_opacity_delta!=0 && dim==1 && c_opacity2>0x30 && c_opacity2<0x42))
						{
							one_done=true;
							if(xmb_txt_buf_max>=MAX_XMB_TEXTS) {redraw_column_texts(_xmb_icon); xmb_txt_buf_max=0;}
							_xmb[_xmb_icon].member[cn].data=xmb_txt_buf_max;
							draw_xmb_title(xmb_txt_buf[xmb_txt_buf_max].data, _xmb[_xmb_icon].member, cn, COL_XMB_TITLE, COL_XMB_SUBTITLE, _xmb_icon);
							xmb_txt_buf_max++;
						}
						if(_xmb[_xmb_icon].member[cn].data!=-1 && ((ss_timer<dim_setting && dim_setting) || _xmb[_xmb_icon].first==cn || dim_setting==0) && abs(_xmb_x_offset)<100/* || (_xmb_icon != _xmb_icon_ && abs(_xmb_x_offset)>150))*/)
						{
							u8 xo1=(_xmb_y_offset>0 ? 1 : 0);
							u8 xo2=1-xo1;
							if( ((_xmb_icon==6 || _xmb_icon==7) && ((cn>=_xmb[_xmb_icon].first-xo1 && cn<=_xmb[_xmb_icon].first+xo2 && _xmb_y_offset!=0) || cn==_xmb[_xmb_icon].first) ) 
								|| (_xmb_icon!=6 && _xmb_icon!=7))
							{

								set_texture(xmb_txt_buf[_xmb[_xmb_icon].member[cn].data].data, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT); //text
								if(_xmb_icon!=6 && _xmb_icon!=7)
									display_img(xpos+((_xmb_icon==3 || _xmb_icon==5 || _xmb_icon==8)?(230+(((V_WIDTH==720)?60:0))):(128+tw/2)), ypos+th/2-XMB_TEXT_HEIGHT/2, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT, 0.5f, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT); //(int)(XMB_TEXT_WIDTH*(1.f-abs((float)_xmb_x_offset)/200.f))
								else
									display_img(xpos+128+tw/2, ypos+th/2-XMB_TEXT_HEIGHT/2, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT, 0.5f, XMB_TEXT_WIDTH, XMB_TEXT_HEIGHT); //(int)(XMB_TEXT_WIDTH*(1.f-abs((float)_xmb_x_offset)/200.f))

							}
						}
					}
				}
			}


		}
		else
		{
			if(egg)
			{
				if(n==xmb_icon-1 && _xmb_x_offset>0) display_img_angle(xpos-(mo_of)/2, 230-(mo_of), 128+mo_of, 128+mo_of, 128, 128, 0.0f, 128, 128, angle);
				else if(n==xmb_icon+1 && _xmb_x_offset<0) display_img_angle(xpos-(mo_of)/2, 230-(mo_of), 128+mo_of, 128+mo_of, 128, 128, 0.0f, 128, 128, angle);
				else display_img_angle(xpos, 230, 128, 128, 128, 128, 0.0f, 128, 128, angle);
			}
			else
			{
				if(n==xmb_icon-1 && _xmb_x_offset>0) display_img(xpos-(mo_of)/2, 230-(mo_of), 128+mo_of, 128+mo_of, 128, 128, 0.0f, 128, 128);
				else if(n==xmb_icon+1 && _xmb_x_offset<0) display_img(xpos-(mo_of)/2, 230-(mo_of), 128+mo_of, 128+mo_of, 128, 128, 0.0f, 128, 128);
				else display_img(xpos, 230, 128, 128, 128, 128, 0.0f, 128, 128);
			}

		}

	}

	if(is_video_loading || is_music_loading || is_photo_loading || is_retro_loading || is_game_loading || is_any_xmb_column)
	{
		if(is_any_xmb_column)
			set_texture(xmb_icon_help, 128, 128);
		else
			set_texture(_xmb[0].data, 128, 128);
		display_img_angle(1770, 74, 64, 64, 128, 128, 0.6f, 128, 128, angle);

		if(time(NULL)&1)
		{
			if(is_any_xmb_column) set_texture(_xmb[is_any_xmb_column].data, 128, 128);
			else if(is_game_loading) set_texture(_xmb[6].data, 128, 128);
			else if(is_video_loading) set_texture(_xmb[5].data, 128, 128);
			else if(is_music_loading) set_texture(_xmb[4].data, 128, 128);
			else if(is_photo_loading) set_texture(_xmb[3].data, 128, 128);
			else if(is_retro_loading) set_texture(_xmb[8].data, 128, 128);

			display_img(1834, 74, 64, 64, 128, 128, 0.6f, 128, 128);
		}
	}
	else
		if((ftp_clients && time(NULL)&2) || (http_active && time(NULL)&1) )
		{
			set_texture((http_active?xmb[9].data:xmb_icon_ftp), 128, 128);
			display_img(1770, 74, 64, 64, 128, 128, 0.0f, 128, 128);
			/*u8 pZ=32;
			if(angle<180.f)
				pZ=(int)((angle/180.f) * 48.f);
			else
				pZ=(int)(((360.f/angle) - 1.f) * 48.f);
			display_img(1770+(48-pZ)/2, 74+(48-pZ)/2, 16+pZ, 16+pZ, 128, 128, 0.6f, 128, 128);*/
			//set_texture(_xmb[0].data, 128, 128);
			//display_img_angle(1770, 74, 64, 64, 128, 128, 0.0f, 128, 128, angle);

		}
}


void draw_xmb_bare(u8 _xmb_icon, u8 _all_icons, bool recursive, int _sub_level)
{

	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	if(xmb_sparks!=0) draw_stars();
	set_texture( text_bmp, 1920, 1080);
	display_img(0, 0, 1920, 1080, 1920, 1080, 0.9f, 1920, 1080);
	draw_xmb_clock(xmb_clock, (_all_icons!=2 ? _xmb_icon : -1));

	if(_all_icons==1) draw_xmb_icons(xmb, _xmb_icon, xmb_slide, xmb_slide_y, recursive, _sub_level);
	else if(_all_icons==2) draw_xmb_icons(xmb, _xmb_icon, xmb_slide, xmb_slide_y, recursive, -1);
	flip();
}


int open_theme_menu(char *_caption, int _width, theme_def *list, int _max, int _x, int _y, int _max_entries, int _centered)
{
	(void) _x;
	(void) _y;
	char filename[1024];

	if(_max_entries>16) _max_entries=16;
	u8 *text_LIST = NULL;
	u8 *text_LIST2 = NULL;
	text_LIST = text_FONT;
	text_LIST2 = text_FONT + 3024000;
	int line_h = 30;
	int _height = (_max_entries+5) * line_h;
	char tdl[512];

	int last_sel=-1;
	int first=0;
	int sel=0;
	sprintf(filename, "%s/LBOX.PNG", app_usrdir);
	load_texture(text_LIST+1512000, filename, 600);
	change_opacity(text_LIST+1512000, 50, 600*630*4);
	sprintf(filename, "%s/LBOX2.PNG", app_usrdir);
	load_texture(text_LIST2+1713600, filename, 680); //4737600
	while(1)
	{

		pad_read();
		if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) ) return -1;
		if ( (new_pad & BUTTON_CROSS) )  return sel;

		if ( (new_pad & BUTTON_DOWN))
		{
			sel++;
			if(sel>=_max) sel=0;
			first=sel-_max_entries+2;
			if(first<0) first=0;
		}

		if ( (new_pad & BUTTON_UP))
		{
			sel--;
			if(sel<0) sel=_max-1;
			first=sel-_max_entries+2;
			if(first<0) first=0;
		}

		if(last_sel!=sel)
		{
//			memset(text_LIST, 0x40, (_width * _height * 4)); 
//			for(int fsr=0; fsr<(_width*_height*4); fsr+=4) *(uint32_t*) ( (u8*)(text_LIST)+fsr )=0x222222a0;
			memcpy(text_LIST, text_LIST+1512000, 1512000);
			memcpy(text_LIST2, text_LIST2+1713600, 1713600);
			max_ttf_label=0;
			print_label_ex( 0.5f, 0.05f, 1.0f, COL_XMB_COLUMN, _caption, 1.04f, 0.0f, 15, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1);

			for(int n=first; (n<(first+_max_entries-1) && n<_max); n++)
			{
				if(_centered)
				{
					if(n==sel)
						print_label_ex( 0.5f, ((float)((n-first+3)*line_h)/(float)_height)-0.011f, 1.4f, 0xf0e0e0e0, list[n].name, 1.04f, 0.0f, 15, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1);
					else
						print_label_ex( 0.5f, ((float)((n-first+3)*line_h)/(float)_height), 1.0f, COL_XMB_SUBTITLE, list[n].name, 1.00f, 0.0f, 15, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1 );
				}
				else
				{
					if(n==sel)
						print_label_ex( 0.05f, ((float)((n-first+3)*line_h)/(float)_height)-0.011f, 1.4f, 0xf0e0e0e0, list[n].name, 1.04f, 0.0f, 15, 0.8f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 0);
					else
						print_label_ex( 0.05f, ((float)((n-first+3)*line_h)/(float)_height), 1.0f, COL_XMB_SUBTITLE, list[n].name, 1.00f, 0.0f, 15, 0.8f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 0 );
				}

			}

			print_label_ex( 0.7f, ((float)(_height-line_h*2)/(float)_height)+0.01f, 1.5f, 0xf0c0c0c0, (char*)"Download", 1.00f, 0.0f, 15, 0.5f/((float)(_width/1920.f)), 0.5f/((float)(_height/1080.f)), 0);
			flush_ttf(text_LIST, _width, _height);
			put_texture_with_alpha_gen( text_LIST, text_DOX+(dox_cross_x	*4 + dox_cross_y	* dox_width*4), dox_cross_w,	dox_cross_h,	dox_width, _width, (int)((0.7f*_width)-dox_cross_w-5), _height-line_h*2);

			sprintf(tdl, "%s/%s.jpg", themes_web_dir, list[sel].name);
			if(stat(tdl, &s3)<0) download_file(list[sel].img, tdl, 0);
			if(stat(tdl, &s3)>=0)
			{
				load_jpg_texture(text_LIST2+20*4+145*680*4, tdl,  680);
			}
			print_label_ex( 0.5f, 0.05f, 1.0f, COL_XMB_COLUMN, list[sel].name, 1.04f, 0.0f, 15, 1.0f/((float)(680/1920.f)), 1.0f/((float)(_height/1080.f)), 1);
			print_label_ex( 0.5f, 0.10f, 1.0f, COL_XMB_SUBTITLE, (char*)"by", 1.00f, 0.0f, 15, 1.0f/((float)(680/1920.f)), 1.0f/((float)(_height/1080.f)), 1);
			print_label_ex( 0.5f, 0.15f, 1.0f, COL_XMB_SUBTITLE, list[sel].author, 1.00f, 0.0f, 15, 1.0f/((float)(680/1920.f)), 1.0f/((float)(_height/1080.f)), 1);

			sprintf(tdl, "%s", list[sel].info);
			if(strlen(tdl)>1)
			print_label_ex( 0.05f, 0.85f, 1.0f, COL_XMB_SUBTITLE, tdl, 0.50f, 0.0f, 15, 1.0f/((float)(680/1920.f)), 1.0f/((float)(_height/1080.f)), 0);
			sprintf(tdl, "multiMAN version: %s", list[sel].mmver);
			print_label_ex( 0.05f, 0.90f, 1.0f, COL_XMB_SUBTITLE, tdl, 0.50f, 0.0f, 15, 1.0f/((float)(680/1920.f)), 1.0f/((float)(_height/1080.f)), 0);

			flush_ttf(text_LIST2, 680, _height);

			last_sel=sel;
		}

		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		if(xmb_sparks!=0) draw_stars();

		set_texture( text_bmp, 1920, 1080);
		display_img(0, 0, 1920, 1080, 1920, 1080, 0.9f, 1920, 1080);

		draw_xmb_clock(xmb_clock, 0);
		draw_xmb_icons(xmb, xmb_icon, xmb_slide, xmb_slide_y, 0, xmb_sublevel);

		set_texture(text_LIST, _width, _height);
//		display_img(_x, _y, _width, _height, _width, _height, 0.0f, _width, _height);
		display_img((int)(mouseX*1920.f), (int)(mouseY*1080.f), _width, _height, _width, _height, 0.0f, _width, _height);
		set_texture(text_LIST+3024000, 680, 630);
		display_img((int)(mouseX*1920.f)+_width+(V_WIDTH==1920?20:70), (int)(mouseY*1080.f), 680, _height, 680, _height, 0.0f, 680, _height);

		flip();

		//cellSysutilCheckCallback();

		mouseX+=mouseXD; mouseY+=mouseYD;
		if(mouseX>0.995f) {mouseX=0.995f;mouseXD=0.0f;} if(mouseX<0.0f) {mouseX=0.0f;mouseXD=0.0f;}
		if(mouseY>0.990f) {mouseY=0.990f;mouseYD=0.0f;} if(mouseY<0.0f) {mouseY=0.0f;mouseYD=0.0f;}

	}

	return -1;
}


void change_opacity(u8 *buffer, int delta, u32 size)
{
	u32 pixel;
	u32 delta2;
	if(delta>0)
	{
		for(u32 fsr=0; fsr<size; fsr+=4) 
		{
			pixel=*(uint32_t*) ((uint8_t*)(buffer)+fsr);
			delta2 = ((u32)((float)(pixel&0xff)*(1.0f+(float)delta/100.f)));
			if(delta2>0xff) delta2=0xff;
			pixel= (pixel & 0xffffff00) | delta2;
			*(uint32_t*) ((uint8_t*)(buffer)+fsr)= pixel;
		}
	}
	else
	{
		for(u32 fsr=0; fsr<size; fsr+=4) 
		{
			pixel=*(uint32_t*) ((uint8_t*)(buffer)+fsr);
			delta2 = ((u32)((float)(pixel&0xff)*((float)abs(delta)/100.f)));
			if(delta2>0xff) delta2=0xff;
			pixel= (pixel & 0xffffff00) | delta2;
			*(uint32_t*) ((uint8_t*)(buffer)+fsr)= pixel;
		}
	}

}

int open_select_menu(char *_caption, int _width, t_opt_list *list, int _max, u8 *buffer, int _max_entries, int _centered)
{
	if(_max_entries>16) _max_entries=16;
	u8 *text_LIST = NULL;
	text_LIST = text_bmpUBG + (1920*1080*4);
	int line_h = 30;
	int _height = (_max_entries+5) * line_h;

	int last_sel=-1;
	int first=0;
	int sel=0;
	char filename[1024];

	sprintf(filename, "%s/LBOX.PNG", app_usrdir);
	load_texture(text_LIST+1512000, filename, 600);
	change_opacity(text_LIST+1512000, 50, 600*630*4);
	while(1)
	{

		pad_read();
		if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) ) return -1;
		if ( (new_pad & BUTTON_CROSS) )  return sel;

		if ( (new_pad & BUTTON_DOWN))
		{
			sel++;
			if(sel>=_max) sel=0;
			first=sel-_max_entries+2;
			if(first<0) first=0;
		}

		if ( (new_pad & BUTTON_UP))
		{
			sel--;
			if(sel<0) sel=_max-1;
			first=sel-_max_entries+2;
			if(first<0) first=0;
		}

		if(last_sel!=sel)
		{
//			memset(text_LIST, 0x40, (_width * _height * 4)); 
//			for(int fsr=0; fsr<(_width*_height*4); fsr+=4) *(uint32_t*) ( (u8*)(text_LIST)+fsr )=0x222222a0;
			memcpy(text_LIST, text_LIST+1512000, 1512000);
			max_ttf_label=0;
			print_label_ex( 0.5f, 0.05f, 1.0f, COL_XMB_COLUMN, _caption, 1.04f, 0.0f, 15, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1);

			for(int n=first; (n<(first+_max_entries-1) && n<_max); n++)
			{
				if(_centered)
				{
					if(n==sel)
						print_label_ex( 0.5f, ((float)((n-first+3)*line_h)/(float)_height)-0.011f, 1.4f, 0xffe0e0e0, list[n].label, 1.04f, 0.0f, 15, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1);
					else
						print_label_ex( 0.5f, ((float)((n-first+3)*line_h)/(float)_height), 1.0f, COL_XMB_SUBTITLE, list[n].label, 1.00f, 0.0f, 15, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1 );
				}
				else
				{
					if(n==sel)
						print_label_ex( 0.05f, ((float)((n-first+3)*line_h)/(float)_height)-0.011f, 1.4f, 0xf0e0e0e0, list[n].label, 1.04f, 0.0f, 15, 0.8f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 0);
					else
						print_label_ex( 0.05f, ((float)((n-first+3)*line_h)/(float)_height), 1.0f, COL_XMB_SUBTITLE, list[n].label, 1.00f, 0.0f, 15, 0.8f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 0 );
				}
			}

			print_label_ex( 0.8f, ((float)(_height-line_h*2)/(float)_height)+0.01f, 1.5f, 0xf0c0c0c0, (char*)"Apply", 1.00f, 0.0f, 15, 0.5f/((float)(_width/1920.f)), 0.5f/((float)(_height/1080.f)), 0);
			flush_ttf(text_LIST, _width, _height);
			put_texture_with_alpha_gen( text_LIST, text_DOX+(dox_cross_x	*4 + dox_cross_y	* dox_width*4), dox_cross_w,	dox_cross_h,	dox_width, _width, (int)((0.8f*_width)-dox_cross_w-5), _height-line_h*2);
			last_sel=sel;
		}

		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

		set_texture( buffer, 1920, 1080);
		display_img(0, 0, 1920, 1080, 1920, 1080, 0.9f, 1920, 1080);

		set_texture(text_LIST, _width, _height);
		display_img((int)(mouseX*1920.f), (int)(mouseY*1080.f), _width, _height, _width, _height, 0.0f, _width, _height);

		flip();
		//cellSysutilCheckCallback();

		mouseX+=mouseXD; mouseY+=mouseYD;
		if(mouseX>0.995f) {mouseX=0.995f;mouseXD=0.0f;} if(mouseX<0.0f) {mouseX=0.0f;mouseXD=0.0f;}
		if(mouseY>0.990f) {mouseY=0.990f;mouseYD=0.0f;} if(mouseY<0.0f) {mouseY=0.0f;mouseYD=0.0f;}

	}

	return -1;
}

int open_list_menu(char *_caption, int _width, t_opt_list *list, int _max, int _x, int _y, int _max_entries, int _centered)
{
	(void) _x;
	(void) _y;
	char filename[1024];

	if(_max_entries>16) _max_entries=16;
	u8 *text_LIST = NULL;
	text_LIST = text_FONT;
	int line_h = 30;
	int _height = (_max_entries+5) * line_h;

	int last_sel=-1;
	int first=0;
	int sel=0;
	sprintf(filename, "%s/LBOX.PNG", app_usrdir);
	load_texture(text_LIST+1512000, filename, 600);
	change_opacity(text_LIST+1512000, 50, 600*630*4);
	while(1)
	{

		pad_read();
		if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) ) return -1;
		if ( (new_pad & BUTTON_CROSS) )  return sel;

		if ( (new_pad & BUTTON_DOWN))
		{
			sel++;
			if(sel>=_max) sel=0;
			first=sel-_max_entries+2;
			if(first<0) first=0;
		}

		if ( (new_pad & BUTTON_UP))
		{
			sel--;
			if(sel<0) sel=_max-1;
			first=sel-_max_entries+2;
			if(first<0) first=0;
		}

		if(last_sel!=sel)
		{
//			memset(text_LIST, 0x40, (_width * _height * 4)); 
//			for(int fsr=0; fsr<(_width*_height*4); fsr+=4) *(uint32_t*) ( (u8*)(text_LIST)+fsr )=0x222222a0;
			memcpy(text_LIST, text_LIST+1512000, 1512000);
			max_ttf_label=0;
			print_label_ex( 0.5f, 0.05f, 1.0f, COL_XMB_COLUMN, _caption, 1.04f, 0.0f, 15, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1);

			for(int n=first; (n<(first+_max_entries-1) && n<_max); n++)
			{
				if(_centered)
				{
					if(n==sel)
						print_label_ex( 0.5f, ((float)((n-first+3)*line_h)/(float)_height)-0.011f, 1.4f, 0xf0e0e0e0, list[n].label, 1.04f, 0.0f, 15, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1);
					else
						print_label_ex( 0.5f, ((float)((n-first+3)*line_h)/(float)_height), 1.0f, COL_XMB_SUBTITLE, list[n].label, 1.00f, 0.0f, 15, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1 );
				}
				else
				{
					if(n==sel)
						print_label_ex( 0.05f, ((float)((n-first+3)*line_h)/(float)_height)-0.011f, 1.4f, 0xf0e0e0e0, list[n].label, 1.04f, 0.0f, 15, 0.8f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 0);
					else
						print_label_ex( 0.05f, ((float)((n-first+3)*line_h)/(float)_height), 1.0f, COL_XMB_SUBTITLE, list[n].label, 1.00f, 0.0f, 15, 0.8f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 0 );
				}
			}

			print_label_ex( 0.8f, ((float)(_height-line_h*2)/(float)_height)+0.01f, 1.5f, 0xf0c0c0c0, (char*)"Apply", 1.00f, 0.0f, 15, 0.5f/((float)(_width/1920.f)), 0.5f/((float)(_height/1080.f)), 0);
			flush_ttf(text_LIST, _width, _height);
			put_texture_with_alpha_gen( text_LIST, text_DOX+(dox_cross_x	*4 + dox_cross_y	* dox_width*4), dox_cross_w,	dox_cross_h,	dox_width, _width, (int)((0.8f*_width)-dox_cross_w-5), _height-line_h*2);
			last_sel=sel;
		}

		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		if(xmb_sparks!=0) draw_stars();

		set_texture( text_bmp, 1920, 1080);
		display_img(0, 0, 1920, 1080, 1920, 1080, 0.9f, 1920, 1080);

		draw_xmb_clock(xmb_clock, 0);
		draw_xmb_icons(xmb, xmb_icon, xmb_slide, xmb_slide_y, 0, xmb_sublevel);

		set_texture(text_LIST, _width, _height);
//		display_img(_x, _y, _width, _height, _width, _height, 0.0f, _width, _height);
		display_img((int)(mouseX*1920.f), (int)(mouseY*1080.f), _width, _height, _width, _height, 0.0f, _width, _height);


		flip();
		//cellSysutilCheckCallback();

		mouseX+=mouseXD; mouseY+=mouseYD;
		if(mouseX>0.995f) {mouseX=0.995f;mouseXD=0.0f;} if(mouseX<0.0f) {mouseX=0.0f;mouseXD=0.0f;}
		if(mouseY>0.990f) {mouseY=0.990f;mouseYD=0.0f;} if(mouseY<0.0f) {mouseY=0.0f;mouseYD=0.0f;}

	}

	return -1;
}

int open_dd_menu(char *_caption, int _width, t_opt_list *list, int _max, int _x, int _y, int _max_entries)
{
	(void) _x;
	(void) _y;

	if(_max_entries>16) _max_entries=16;
	u8 *text_LIST = NULL;
	text_LIST = text_FONT;
	int line_h = 26;
	int _height = 315;//(_max_entries+5) * line_h;

	int last_sel=-1;
	int first=0;
	int sel=0;
	char filename[1024];
	char string1[1024];

	sprintf(filename, "%s/LBOX.PNG", app_usrdir);
	load_texture(text_LIST+756000, filename, 600);
	mip_texture(text_LIST+756000, text_LIST+756000, 600, 630, -2);

	while(1)
	{

		pad_read();
		if ( (new_pad & BUTTON_TRIANGLE) || (new_pad & BUTTON_CIRCLE) ) return -1;
		if ( (new_pad & BUTTON_CROSS) )  return sel;

		if ( (new_pad & BUTTON_DOWN))
		{
			sel++;
			if(sel>=_max) sel=0;
			first=sel-_max_entries+2;
			if(first<0) first=0;
		}

		if ( (new_pad & BUTTON_UP))
		{
			sel--;
			if(sel<0) sel=_max-1;
			first=sel-_max_entries+2;
			if(first<0) first=0;
		}

		if(last_sel!=sel)
		{
//			memset(text_LIST, 0x40, (_width * _height * 4)); 
//			for(int fsr=0; fsr<(_width*_height*4); fsr+=4) *(uint32_t*) ( (u8*)(text_LIST)+fsr )=0x222222a0;
			memcpy(text_LIST, text_LIST+756000, 756000);
			max_ttf_label=0;
			print_label_ex( 0.5f, 0.05f, 0.7f, COL_XMB_COLUMN, _caption, 1.04f, 0.0f, 0, 1.0f/((float)(_width/1920.f)), 1.0f/((float)(_height/1080.f)), 1);
			flush_ttf(text_LIST, _width, _height);

			for(int n=first; (n<(first+_max_entries-1) && n<_max); n++)
			{
				if(n==sel)
					print_label_ex( 0.055f, ((float)((n-first+2.2f)*line_h)/(float)_height)-0.007f, 1.2f, 0xf0e0e0e0, list[n].label, 1.04f, 0.0f, 17, 0.68f/((float)(_width/1920.f)), 0.85f/((float)(_height/1080.f)), 0);
				else
					print_label_ex( 0.120f, ((float)((n-first+2.2f)*line_h)/(float)_height), 0.85f, COL_XMB_SUBTITLE, list[n].label, 1.04f, 0.0f, 17, 0.8f/((float)(_width/1920.f)), 0.95f/((float)(_height/1080.f)), 0 );
				flush_ttf(text_LIST, _width, _height);
			}

			print_label_ex( 0.6f, ((float)(_height-line_h*2)/(float)_height)+0.0326f, 1.5f, 0xf0c0c0c0, (char*)"Confirm", 1.04f, 0.0f, 15, 0.5f/((float)(_width/1920.f)), 0.5f/((float)(_height/1080.f)), 0);
			flush_ttf(text_LIST, _width, _height);
			put_texture_with_alpha_gen( text_LIST, text_DOX+(dox_cross_x	*4 + dox_cross_y	* dox_width*4), dox_cross_w,	dox_cross_h,	dox_width, _width, (int)((0.6f*_width)-dox_cross_w-5), _height-line_h*2+4);
			last_sel=sel;
		}

		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

//		setRenderColor();
		draw_fileman();

		set_texture(text_LIST, _width, _height);
		display_img((int)(mouseX*1920.f), (int)(mouseY*1080.f), _width, _height, _width, _height, -0.3f, _width, _height);

		time ( &rawtime );	timeinfo = localtime ( &rawtime );
		if(date_format==0) sprintf(string1, "%02d/%02d/%04d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
		else if(date_format==1) sprintf(string1, "%02d/%02d/%04d", timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_year+1900);
		else if(date_format==2) sprintf(string1, "%04d/%02d/%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday );

		cellDbgFontPrintf( 0.83f, 0.895f, 0.70f ,COL_HEXVIEW, "%s\n %s:%02d:%02d ", string1, tmhour(timeinfo->tm_hour), timeinfo->tm_min, timeinfo->tm_sec);

		flip();
		//cellSysutilCheckCallback();

		mouseX+=mouseXD; mouseY+=mouseYD;
		if(mouseX>0.995f) {mouseX=0.995f;mouseXD=0.0f;} if(mouseX<0.0f) {mouseX=0.0f;mouseXD=0.0f;}
		if(mouseY>0.990f) {mouseY=0.990f;mouseYD=0.0f;} if(mouseY<0.0f) {mouseY=0.0f;mouseYD=0.0f;}

	}

	return -1;
}

static void add_video_column_thread_entry( uint64_t arg )
{
	(void)arg;

	if(init_finished && is_video_loading && xmb[5].init)
	{
		t_dir_pane_bare *pane =  (t_dir_pane_bare *) malloc(sizeof(t_dir_pane_bare)*MAX_PANE_SIZE_BARE);

		int max_dir=0;
		char linkfile[512];
		char imgfile[512];
		char imgfile2[512];
		char filename[1024];

		sprintf(filename, "%s/XMB Video", app_usrdir);
		if(stat(filename, &s3)<0) mkdir(filename, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
		else del_temp(filename);

		ps3_home_scan_bare((char*)"/dev_hdd0/video", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_hdd0/VIDEO", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_sd/VIDEO", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_sd/DCIM", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_ms/VIDEO", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_cf/VIDEO", pane, &max_dir); 
		for(int ret_f=0; ret_f<9; ret_f++)
		{
			sprintf(linkfile, "/dev_usb00%i/VIDEO", ret_f);
			ps3_home_scan_bare(linkfile, pane, &max_dir); 
		} 
		for(int ret_f=0; ret_f<max_dir; ret_f++)
			if(is_video(pane[ret_f].name))
			{

				sprintf(linkfile, "%s/%s", pane[ret_f].path, pane[ret_f].name);
				if(strstr(linkfile, "/dev_hdd0")!=NULL)
				{
					sprintf(filename, "%s/XMB Video/%s", app_usrdir, pane[ret_f].name);
					link(linkfile, filename);
				}

				if(pane[ret_f].name[strlen(pane[ret_f].name)-4]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-4]=0;
				else if(pane[ret_f].name[strlen(pane[ret_f].name)-5]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-5]=0;

				sprintf(imgfile2, "%s.jpg", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok;
				sprintf(imgfile2, "%s.JPG", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok;

				sprintf(imgfile, "%s", linkfile);
				if(imgfile[strlen(imgfile)-4]=='.') imgfile[strlen(imgfile)-4]=0;
				if(imgfile[strlen(imgfile)-5]=='.') imgfile[strlen(imgfile)-5]=0;
				sprintf(imgfile2, "%s.STH", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok;
				sprintf(imgfile2, "%s.jpg", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok;
				sprintf(imgfile2, "%s.JPG", imgfile); //if(stat(imgfile2, &s3)>=0) goto thumb_ok;

thumb_ok:
				if(stat(imgfile2, &s3)>=0)
				{
					add_xmb_member(xmb[5].member, &xmb[5].size, pane[ret_f].name, (strstr(linkfile, "/dev_hdd0")==NULL ? (char*) "Video" : (char*)"HDD Video"),
					/*type*/3, /*status*/0, /*game_id*/-1, /*icon*/xmb_icon_film, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)imgfile2, 0, 0);
				}
				else
					add_xmb_member(xmb[5].member, &xmb[5].size, pane[ret_f].name, (strstr(linkfile, "/dev_hdd0")==NULL ? (char*) "Video" : (char*)"HDD Video"),
					/*type*/3, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_film, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)"/", 0, 0);

				//if(xmb[5].size<9)	draw_xmb_bare(5, 1, 0, 0);
				if(xmb[5].size>=MAX_XMB_MEMBERS) break;
				sort_xmb_col(xmb[5].member, xmb[5].size, 2);
		}
		free_all_buffers();
		free(pane);

	}
	if(xmb_icon_last==5 && (xmb_icon_last_first<xmb[5].size)) { xmb[5].first=xmb_icon_last_first; xmb_icon_last_first=0;xmb_icon_last=0; }

	is_video_loading=0;
	sys_ppu_thread_exit(0);
}

static void add_photo_column_thread_entry( uint64_t arg )
{
	(void)arg;

	if(init_finished && is_photo_loading && xmb[3].init)
	{
		char linkfile[512];
		xmb[3].first=0;	
		xmb[3].size=0;
		xmb[3].init=1;
		char t_ip[512];
		int i_status;

		t_dir_pane_bare *pane =  (t_dir_pane_bare *) malloc(sizeof(t_dir_pane_bare)*MAX_PANE_SIZE_BARE);
		int max_dir=0;

		ps3_home_scan_bare((char*)"/dev_hdd0/photo", pane, &max_dir); 
		for(int ret_f=0; ret_f<9; ret_f++)
		{
			sprintf(linkfile, "/dev_usb00%i/PICTURE", ret_f);
			ps3_home_scan_bare(linkfile, pane, &max_dir); 
		}

		ps3_home_scan_bare((char*)"/dev_sd/PICTURE", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_sd/DCIM", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_ms/PICTURE", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_cf/PICTURE", pane, &max_dir); 

		for(int ret_f=0; ret_f<max_dir; ret_f++)
		{
			if(strstr(pane[ret_f].name, ".jpg")!=NULL || strstr(pane[ret_f].name, ".JPG")!=NULL || strstr(pane[ret_f].name, ".png")!=NULL || strstr(pane[ret_f].name, ".PNG")!=NULL)
			{
				i_status=2;
				sprintf(linkfile, "%s/%s", pane[ret_f].path, pane[ret_f].name);
				if(strstr(linkfile, ".JPG")!=NULL || strstr(linkfile, ".JPEG")!=NULL || strstr(linkfile, ".jpg")!=NULL || strstr(linkfile, ".jpeg")!=NULL) {sprintf(t_ip, "%s", linkfile); i_status=0;} else {sprintf(t_ip, "%s", "/");i_status=2;}
				if(pane[ret_f].name[strlen(pane[ret_f].name)-4]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-4]=0;
				else if(pane[ret_f].name[strlen(pane[ret_f].name)-5]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-5]=0;
				add_xmb_member(xmb[3].member, &xmb[3].size, pane[ret_f].name, (strstr(linkfile, "/dev_hdd0")==NULL ? (char*) "Photo" : pane[ret_f].path+16),
				/*type*/5, /*status*/i_status, /*game_id*/-1, /*icon*/xmb_icon_photo, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)t_ip, 0, 0);
				//if(xmb[3].size<12)	draw_xmb_bare(3, 1, 0, 0);
				if(xmb[3].size>=MAX_XMB_MEMBERS) break;
			}
		}

		sort_xmb_col(xmb[3].member, xmb[3].size, 0);	
		free_all_buffers();
		free(pane);
	}

	is_photo_loading=0;
	if(xmb_icon_last==3 && (xmb_icon_last_first<xmb[3].size)) { xmb[3].first=xmb_icon_last_first; xmb_icon_last_first=0;xmb_icon_last=0; }
	sys_ppu_thread_exit(0);
}


static void add_music_column_thread_entry( uint64_t arg )
{
	(void)arg;

	if(init_finished && is_music_loading && xmb[4].init)
	{
		t_dir_pane_bare *pane =  (t_dir_pane_bare *) malloc(sizeof(t_dir_pane_bare)*MAX_PANE_SIZE_BARE);
		int max_dir=0;

		char linkfile[512];
		xmb[4].first=0;	
		xmb[4].size=0;
		xmb[4].init=1;
		u8 skip_first=0;


		ps3_home_scan_bare((char*)"/dev_hdd0/music", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_hdd0/MUSIC", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_sd/MUSIC", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_ms/MUSIC", pane, &max_dir); 
		ps3_home_scan_bare((char*)"/dev_cf/MUSIC", pane, &max_dir); 
		for(int ret_f=0; ret_f<9; ret_f++)
		{
			sprintf(linkfile, "/dev_usb00%i/MUSIC", ret_f);
			ps3_home_scan_bare(linkfile, pane, &max_dir); 
		}
		for(int ret_f=0; ret_f<max_dir; ret_f++)
		{
			if(strstr(pane[ret_f].name, ".mp3")!=NULL || strstr(pane[ret_f].name, ".MP3")!=NULL || strstr(pane[ret_f].name, ".ac3")!=NULL || strstr(pane[ret_f].name, ".AC3")!=NULL || strstr(pane[ret_f].name, ".FLAC")!=NULL || strstr(pane[ret_f].name, ".flac")!=NULL)
			{
				sprintf(linkfile, "%s/%s", pane[ret_f].path, pane[ret_f].name);

				if(pane[ret_f].name[strlen(pane[ret_f].name)-4]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-4]=0;
				else if(pane[ret_f].name[strlen(pane[ret_f].name)-5]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-5]=0;

				skip_first=0;
				for(u8 c=0; c<strlen(pane[ret_f].name); c++)
					if(pane[ret_f].name[c]>0x40) {skip_first=c; break;}

				add_xmb_member(xmb[4].member, &xmb[4].size, pane[ret_f].name+skip_first, (strstr(linkfile, "/dev_hdd0")==NULL ? (char*) "Music" : (char*)"HDD Music"),
				/*type*/4, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_note, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)"/", 0, 0);
				//if(xmb[4].size<9)	draw_xmb_bare(4, 1, 0, 0);
				if(xmb[4].size>=MAX_XMB_MEMBERS) break;
				sort_xmb_col(xmb[4].member, xmb[4].size, 0);
			}
		}

	free(pane);
	}
	is_music_loading=0;
	if(xmb_icon_last==4 && (xmb_icon_last_first<xmb[4].size)) { xmb[4].first=xmb_icon_last_first; xmb_icon_last_first=0;xmb_icon_last=0; }
	sys_ppu_thread_exit(0);
}

static void add_retro_column_thread_entry( uint64_t arg )
{
	(void)arg;

	if(init_finished && is_retro_loading && xmb[8].init)
	{

		t_dir_pane_bare *pane =  (t_dir_pane_bare *) malloc(sizeof(t_dir_pane_bare)*MAX_PANE_SIZE_BARE);
		int max_dir=0;

		xmb[8].size=0;
		add_xmb_member(xmb[8].member, &xmb[8].size, (char*)"Refresh", (char*)"Scan all emulator folders for newly added game ROMs.",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb[0].data, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
		xmb[8].init=1;
		char linkfile[512];
		char imgfile[512];
		char imgfile2[512];

		ps3_home_scan_bare(snes_roms, pane, &max_dir); 
		if(strcmp(snes_roms, "/dev_hdd0/ROMS/snes"))
			ps3_home_scan_bare((char*)"/dev_hdd0/ROMS/snes", pane, &max_dir); 

		for(int ret_f=0; ret_f<9; ret_f++)
		{
			sprintf(linkfile, "/dev_usb00%i/ROMS/snes", ret_f);
			ps3_home_scan_bare(linkfile, pane, &max_dir); 
		}

		for(int ret_f=0; ret_f<max_dir; ret_f++)
			if(is_snes9x(pane[ret_f].name))
			{
				sprintf(linkfile, "%s/%s", pane[ret_f].path, pane[ret_f].name);
				if(pane[ret_f].name[strlen(pane[ret_f].name)-4]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-4]=0;

				sprintf(imgfile, "%s", linkfile);
				if(imgfile[strlen(imgfile)-4]=='.') imgfile[strlen(imgfile)-4]=0;

				sprintf(imgfile2, "%s/snes/%s.jpg", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				sprintf(imgfile2, "%s/snes/%s.png", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				sprintf(imgfile2, "%s.jpg", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				sprintf(imgfile2, "%s.png", imgfile); //if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				//sprintf(imgfile2, "%s.JPG", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				//sprintf(imgfile2, "%s.PNG", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;


				/*sprintf(imgfile2, "%s/snes/%s.JPG", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				sprintf(imgfile2, "%s/snes/%s.PNG", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;

				sprintf(imgfile2, "%s.jpg", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				sprintf(imgfile2, "%s.JPG", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				sprintf(imgfile2, "%s.png", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				sprintf(imgfile2, "%s.PNG", linkfile);// if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes;
				sprintf(imgfile2, "%s/snes/NO_COVER.JPG", covers_retro);// if(stat(imgfile2, &s3)>=0) goto thumb_ok_snes; */


thumb_ok_snes:
				if(stat(imgfile2, &s3)>=0)
				{
					add_xmb_member(xmb[8].member, &xmb[8].size, pane[ret_f].name, (char*)"SNES9x Game ROM",
					/*type*/8, /*status*/0, /*game_id*/-1, /*icon*/xmb_icon_retro, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)imgfile2, 0, 0);
				}
				else
					add_xmb_member(xmb[8].member, &xmb[8].size, pane[ret_f].name, (char*)"SNES9x Game ROM",
					/*type*/8, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_retro, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)"/", 0, 0);

				//if(xmb[8].size<9)	draw_xmb_bare(8, 1, 0, 0);
				if(!(xmb[8].size & 0x0f)) sort_xmb_col(xmb[8].member, xmb[8].size, 1);
				if(xmb[8].size>=MAX_XMB_MEMBERS) break;

			}
				sort_xmb_col(xmb[8].member, xmb[8].size, 1);

//genp
		max_dir=0;
		ps3_home_scan_bare(genp_roms, pane, &max_dir); 
		if(strcmp(genp_roms, "/dev_hdd0/ROMS/gen"))
			ps3_home_scan_bare((char*)"/dev_hdd0/ROMS/gen", pane, &max_dir); 

		for(int ret_f=0; ret_f<9; ret_f++)
		{
			sprintf(linkfile, "/dev_usb00%i/ROMS/gen", ret_f);
			ps3_home_scan_bare(linkfile, pane, &max_dir); 
		}

		for(int ret_f=0; ret_f<max_dir; ret_f++)
			if(is_genp(pane[ret_f].name))
			{
				sprintf(linkfile, "%s/%s", pane[ret_f].path, pane[ret_f].name);
				if(pane[ret_f].name[strlen(pane[ret_f].name)-4]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-4]=0;
				else if(pane[ret_f].name[strlen(pane[ret_f].name)-3]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-3]=0;

				sprintf(imgfile, "%s", linkfile);
				if(imgfile[strlen(imgfile)-4]=='.') imgfile[strlen(imgfile)-4]=0;
				else if(imgfile[strlen(imgfile)-3]=='.') imgfile[strlen(imgfile)-3]=0;
				sprintf(imgfile2, "%s/gen/%s.jpg", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;
				sprintf(imgfile2, "%s/gen/%s.png", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;
				sprintf(imgfile2, "%s.jpg", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;
				sprintf(imgfile2, "%s.png", imgfile);/* if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;
				sprintf(imgfile2, "%s.JPG", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;
				sprintf(imgfile2, "%s.PNG", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;

				sprintf(imgfile2, "%s/gen/%s.JPG", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;
				sprintf(imgfile2, "%s/gen/%s.PNG", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;

				sprintf(imgfile2, "%s.jpg", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;
				sprintf(imgfile2, "%s.JPG", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;
				sprintf(imgfile2, "%s.png", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp;
				sprintf(imgfile2, "%s.PNG", linkfile);// if(stat(imgfile2, &s3)>=0) goto thumb_ok_genp; */

thumb_ok_genp:
				if(xmb[8].size>=MAX_XMB_MEMBERS) break;
				if(stat(imgfile2, &s3)>=0)
				{
					add_xmb_member(xmb[8].member, &xmb[8].size, pane[ret_f].name, (char*)"Genesis+ GX Game ROM",
					/*type*/11, /*status*/0, /*game_id*/-1, /*icon*/xmb_icon_retro, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)imgfile2, 0, 0);
				}
				else
					add_xmb_member(xmb[8].member, &xmb[8].size, pane[ret_f].name, (char*)"Genesis+ GX Game ROM",
					/*type*/11, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_retro, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)"/", 0, 0);

				//if(xmb[8].size<9)	draw_xmb_bare(8, 1, 0, 0);
				if(!(xmb[8].size & 0x0f)) sort_xmb_col(xmb[8].member, xmb[8].size, 1);
				if(xmb[8].size>=MAX_XMB_MEMBERS) break;
			}
			sort_xmb_col(xmb[8].member, xmb[8].size, 1);

//fceu+
		max_dir=0;
		ps3_home_scan_bare(fceu_roms, pane, &max_dir); 
		if(strcmp(fceu_roms, "/dev_hdd0/ROMS/fceu"))
			ps3_home_scan_bare((char*)"/dev_hdd0/ROMS/fceu", pane, &max_dir); 

		for(int ret_f=0; ret_f<9; ret_f++)
		{
			sprintf(linkfile, "/dev_usb00%i/ROMS/fceu", ret_f);
			ps3_home_scan_bare(linkfile, pane, &max_dir); 
		}

		for(int ret_f=0; ret_f<max_dir; ret_f++)
			if(is_fceu(pane[ret_f].name))
			{
				sprintf(linkfile, "%s/%s", pane[ret_f].path, pane[ret_f].name);
				if(pane[ret_f].name[strlen(pane[ret_f].name)-4]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-4]=0;
				else if(pane[ret_f].name[strlen(pane[ret_f].name)-5]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-5]=0;

				sprintf(imgfile, "%s", linkfile);
				if(imgfile[strlen(imgfile)-4]=='.') imgfile[strlen(imgfile)-4]=0;
				else if(imgfile[strlen(imgfile)-5]=='.') imgfile[strlen(imgfile)-5]=0;
				sprintf(imgfile2, "%s/fceu/%s.jpg", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s/fceu/%s.png", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s.jpg", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s.png", imgfile); /* if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s.JPG", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s.PNG", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;


				sprintf(imgfile2, "%s/fceu/%s.JPG", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s/fceu/%s.PNG", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s.jpg", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s.JPG", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s.png", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu;
				sprintf(imgfile2, "%s.PNG", linkfile);// if(stat(imgfile2, &s3)>=0) goto thumb_ok_fceu; */

thumb_ok_fceu:
				if(xmb[8].size>=MAX_XMB_MEMBERS) break;
				if(stat(imgfile2, &s3)>=0)
				{
					add_xmb_member(xmb[8].member, &xmb[8].size, pane[ret_f].name, (char*)"Genesis+ GX Game ROM",
					/*type*/9, /*status*/0, /*game_id*/-1, /*icon*/xmb_icon_retro, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)imgfile2, 0, 0);
				}
				else
					add_xmb_member(xmb[8].member, &xmb[8].size, pane[ret_f].name, (char*)"Genesis+ GX Game ROM",
					/*type*/9, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_retro, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)"/", 0, 0);

				//if(xmb[8].size<9)	draw_xmb_bare(8, 1, 0, 0);
				if(!(xmb[8].size & 0x0f)) sort_xmb_col(xmb[8].member, xmb[8].size, 1);
				if(xmb[8].size>=MAX_XMB_MEMBERS) break;

			}
				sort_xmb_col(xmb[8].member, xmb[8].size, 1);

//vba
		max_dir=0;
		ps3_home_scan_bare(vba_roms, pane, &max_dir); 
		if(strcmp(vba_roms, "/dev_hdd0/ROMS/vba"))
			ps3_home_scan_bare((char*)"/dev_hdd0/ROMS/vba", pane, &max_dir); 

		for(int ret_f=0; ret_f<9; ret_f++)
		{
			sprintf(linkfile, "/dev_usb00%i/ROMS/vba", ret_f);
			ps3_home_scan_bare(linkfile, pane, &max_dir); 
		}

		for(int ret_f=0; ret_f<max_dir; ret_f++)
			if(is_vba(pane[ret_f].name))
			{
				sprintf(linkfile, "%s/%s", pane[ret_f].path, pane[ret_f].name);
				if(pane[ret_f].name[strlen(pane[ret_f].name)-4]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-4]=0;
				else if(pane[ret_f].name[strlen(pane[ret_f].name)-3]=='.') pane[ret_f].name[strlen(pane[ret_f].name)-3]=0;

				sprintf(imgfile, "%s", linkfile);
				if(imgfile[strlen(imgfile)-4]=='.') imgfile[strlen(imgfile)-4]=0;
				else if(imgfile[strlen(imgfile)-3]=='.') imgfile[strlen(imgfile)-3]=0;
				sprintf(imgfile2, "%s/vba/%s.jpg", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;
				sprintf(imgfile2, "%s/vba/%s.png", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;
				sprintf(imgfile2, "%s.jpg", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;
				sprintf(imgfile2, "%s.png", imgfile);/* if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;
				sprintf(imgfile2, "%s.JPG", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;
				sprintf(imgfile2, "%s.PNG", imgfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;

				sprintf(imgfile2, "%s/vba/%s.JPG", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;
				sprintf(imgfile2, "%s/vba/%s.PNG", covers_retro, pane[ret_f].name); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;

				sprintf(imgfile2, "%s.jpg", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;
				sprintf(imgfile2, "%s.JPG", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;
				sprintf(imgfile2, "%s.png", linkfile); if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba;
				sprintf(imgfile2, "%s.PNG", linkfile);// if(stat(imgfile2, &s3)>=0) goto thumb_ok_vba; */

thumb_ok_vba:
				if(xmb[8].size>=MAX_XMB_MEMBERS) break;
				if(stat(imgfile2, &s3)>=0)
				{
					add_xmb_member(xmb[8].member, &xmb[8].size, pane[ret_f].name, (char*)"GameBoy Game ROM",
					/*type*/10, /*status*/0, /*game_id*/-1, /*icon*/xmb_icon_retro, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)imgfile2, 0, 0);
				}
				else
					add_xmb_member(xmb[8].member, &xmb[8].size, pane[ret_f].name, (char*)"GameBoy Game ROM",
					/*type*/10, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_retro, 128, 128, /*f_path*/(char*)linkfile, /*i_path*/(char*)"/", 0, 0);

				if(!(xmb[8].size & 0x0f)) sort_xmb_col(xmb[8].member, xmb[8].size, 1);
				if(xmb[8].size>=MAX_XMB_MEMBERS) break;
			}
		sort_xmb_col(xmb[8].member, xmb[8].size, 1);


		if(xmb[8].size>2) xmb[8].first=1;

		free_all_buffers();
		free(pane);
	}

	is_retro_loading=0;
	if(xmb_icon_last==8 && (xmb_icon_last_first<xmb[8].size)) { xmb[8].first=xmb_icon_last_first; xmb_icon_last_first=0;xmb_icon_last=0; }
	sys_ppu_thread_exit(0);
}

void add_photo_column()
{
	if(is_photo_loading || xmb[3].init) return;
	is_photo_loading=1;
	xmb[3].init=1;
	sys_ppu_thread_create( &addpic_thr_id, add_photo_column_thread_entry,
						   0, 
						   misc_thr_prio, app_stack_size,
						   0, "multiMAN_add_photo" );//SYS_PPU_THREAD_CREATE_JOINABLE
}

void add_music_column()
{
	if(is_music_loading || xmb[4].init) return;
	is_music_loading=1;
	xmb[4].init=1;
	sys_ppu_thread_create( &addmus_thr_id, add_music_column_thread_entry,
						   0, 
						   misc_thr_prio, app_stack_size,
						   0, "multiMAN_add_music" );//SYS_PPU_THREAD_CREATE_JOINABLE
}

void add_video_column()
{
	if(is_video_loading || xmb[5].init) return;
//	if(is_video_loading || is_music_loading || is_photo_loading || is_retro_loading) return;
	is_video_loading=1;
	xmb[5].init=1;
	sys_ppu_thread_create( &addvid_thr_id, add_video_column_thread_entry,
						   0, 
						   misc_thr_prio, app_stack_size,
						   0, "multiMAN_add_video" );//SYS_PPU_THREAD_CREATE_JOINABLE
}

void add_emulator_column()
{
	if(is_retro_loading || xmb[8].init) return;
/*
	char string1[1024];
	sprintf(list_file_state, "%s/XMBS.008", app_usrdir);
	FILE *flist = fopen(list_file_state, "rb");
	if(flist!=NULL)
	{
		fread((char*) &string1, 8, 1, flist);
		if(strstr(string1, XMB_COL_VER)!=NULL)
		{
			fseek(flist, 0, SEEK_END);
			int llist_size=ftell(flist)-8;
			fseek(flist, 8, SEEK_SET);
			fread((char*) &xmb[8], llist_size, 1, flist);
			fclose(flist);
			free_all_buffers();
			for(int n=0; n<MAX_XMB_TEXTS; n++)
			{
				xmb_txt_buf[n].used=0;
				xmb_txt_buf[n].data=text_bmpUPSR+(n*XMB_TEXT_WIDTH*XMB_TEXT_HEIGHT*4);
			}
			xmb_txt_buf_max=0;
			for(int n=0; n<xmb[8].size; n++) xmb[8].member[n].data=-1;

			if(xmb[8].size<1)
			{
				remove(list_file_state);
				is_retro_loading=1;
			}
			else
				is_retro_loading=0;
		}
		else
		{
			fclose(flist);
			remove(list_file_state);
			is_retro_loading=1;
		}
	}
	else */
		is_retro_loading=1;

	if(is_retro_loading)
	{
		xmb[8].init=1;
		sys_ppu_thread_create( &addret_thr_id, add_retro_column_thread_entry,
							   0, 
							   misc_thr_prio, app_stack_size,
							   0, "multiMAN_add_retro" );
	}
}


void save_options()
{
	seconds_clock=0;
	remove(options_bin);
	FILE *fpA;
	fpA = fopen ( options_bin, "w" );
	if(fpA!=NULL)
	{
		fprintf(fpA, "download_covers=%i\r\n", download_covers);
		fprintf(fpA, "date_format=%i\r\n", date_format);
		fprintf(fpA, "time_format=%i\r\n", time_format);
		fprintf(fpA, "clear_activity_logs=%i\r\n", clear_activity_logs);
		fprintf(fpA, "ftpd_on=%i\r\n", ftp_service);
		fprintf(fpA, "disable_options=%i\r\n", disable_options);
		fprintf(fpA, "mount_hdd1=%i\r\n", mount_hdd1);
		fprintf(fpA, "scan_avchd=%i\r\n", scan_avchd);
		fprintf(fpA, "expand_avchd=%i\r\n", expand_avchd);
		fprintf(fpA, "xmb_sparks=%i\r\n", xmb_sparks);
		fprintf(fpA, "xmb_popup=%i\r\n", xmb_popup);
		fprintf(fpA, "xmb_game_bg=%i\r\n", xmb_game_bg);
		fprintf(fpA, "xmb_cover=%i\r\n", xmb_cover);
		fprintf(fpA, "verify_data=%i\r\n", verify_data);

		fprintf(fpA, "scan_for_apps=%i\r\n", scan_for_apps);
		fprintf(fpA, "fullpng=%i\r\n", initial_cover_mode);
		fprintf(fpA, "user_font=%i\r\n", user_font);
		fprintf(fpA, "lock_display_mode=%i\r\n", lock_display_mode);
		fprintf(fpA, "lock_fileman=%i\r\n", lock_fileman);
		
		fprintf(fpA, "overscan=%i\r\n", (int)(overscan*100.f));
		fprintf(fpA, "display_mode=%i\r\n", display_mode);
		fprintf(fpA, "showdir=%i\r\n", dir_mode);
		fprintf(fpA, "game_details=%i\r\n", game_details);
		fprintf(fpA, "bd_emulator=%i\r\n", bd_emulator);
		fprintf(fpA, "animation=%i\r\n", animation);
		fprintf(fpA, "game_bg_overlay=%i\r\n", game_bg_overlay);
		fprintf(fpA, "progress_bar=%i\r\n", progress_bar);
		fprintf(fpA, "dim_titles=%i\r\n", dim_setting);
		fprintf(fpA, "ss_timeout=%i\r\n", ss_timeout);
		fprintf(fpA, "deadzone_x=%i\r\n", xDZ);
		fprintf(fpA, "deadzone_y=%i\r\n", yDZ);
		fprintf(fpA, "parental_level=%i\r\n", parental_level);
		fprintf(fpA, "parental_pass=%s\r\n", parental_pass);
		fprintf(fpA, "theme_sound=%i\r\n", theme_sound);
		fprintf(fpA, "gray_poster=%i\r\n", gray_poster);
		fprintf(fpA, "confirm_with_x=%i\r\n", confirm_with_x);
		fprintf(fpA, "hide_bd=%i\r\n", hide_bd);
		
		
//		fprintf(fpA, "%i\n", );

		fclose( fpA);
	}

}
void parse_settings()
{
	seconds_clock=0;
	char oini[32];
	for(int n=0;n<xmb[2].size;n++ )
	{
		if(!xmb[2].member[n].option_size) continue;
		sprintf(oini, "%s", xmb[2].member[n].optionini);

			 if(!strcmp(oini, "download_covers"))	download_covers	=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "date_format"))		date_format		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "time_format"))		time_format		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "clear_activity_logs"))	clear_activity_logs		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "ftpd_on"))			ftp_service		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "disable_options"))	disable_options		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "mount_hdd1"))		mount_hdd1		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);

		else if(!strcmp(oini, "scan_avchd"))		scan_avchd		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "expand_avchd"))		expand_avchd		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);

		else if(!strcmp(oini, "xmb_sparks"))		xmb_sparks		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "xmb_popup"))			xmb_popup		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "xmb_game_bg"))		xmb_game_bg		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "xmb_cover"))			xmb_cover		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "verify_data"))		verify_data		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "scan_for_apps"))		scan_for_apps		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "full_png"))			initial_cover_mode		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "user_font"))			user_font		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "theme_sound"))		theme_sound		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "lock_display_mode"))	lock_display_mode		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "lock_fileman"))		lock_fileman	=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "overscan"))			overscan		=(float)((float)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10)/100.f);
		else if(!strcmp(oini, "display_mode"))		display_mode		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "showdir"))			dir_mode		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "game_details"))		game_details		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "animation"))			animation		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);

		else if(!strcmp(oini, "game_bg_overlay"))	game_bg_overlay		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "gray_poster"))		gray_poster			=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "confirm_with_x"))	confirm_with_x		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "hide_bd"))			hide_bd				=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		
		else if(!strcmp(oini, "progress_bar"))		progress_bar		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "dim_titles"))		dim_setting		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "ss_timeout"))		ss_timeout		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "deadzone_x"))		xDZ		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "deadzone_y"))		yDZ		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);
		else if(!strcmp(oini, "parental_level"))	parental_level		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);

		else if(!strcmp(oini, "bd_emulator"))		bd_emulator		=(int)strtol(xmb[2].member[n].option[xmb[2].member[n].option_selected].value, NULL, 10);

//		else if(!strcmp(oini, "parental_pass"))		sprintf(parental_pass, "%s", xmb[2].member[n].option[xmb[2].member[n].option_selected].value);

		xDZa=(int) (xDZ*128/100);
		yDZa=(int) (yDZ*128/100);

	}
//	save_options();
}

void add_settings_column()
{

//		add_xmb_member(xmb[2].member, &xmb[2].size, (char*)"Edit multiMAN options", (char*)"Not implemented yet",
//				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_tool, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);

		add_xmb_member(xmb[2].member, &xmb[2].size, (char*)"System Information", (char*)"Displays information about your PS3\xE2\x84\xA2 system.",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_tool, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);

		add_xmb_member(xmb[2].member, &xmb[2].size, (char*)"Clear Game Cache Data", (char*)"Removes cache files for selected title.",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_tool, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Download Game Covers", (char*)"Adjusts whether to download missing game covers.", (char*)"download_covers");//, (char*)"Internet connection required.")
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No",		(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Auto",		(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=download_covers;
		xmb[2].member[xmb[2].size-1].icon=xmb[9].data;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"FTP Service", (char*)"Sets FTP startup mode.", (char*)"ftpd_on");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disabled",		(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Auto start",	(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=ftp_service;
		xmb[2].member[xmb[2].size-1].icon=xmb_icon_ftp;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"XMMB Sparks", (char*)"Changes display setting for sparks overlay in XMMB display mode.",	(char*)"xmb_sparks");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Show always",					(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Auto hide",					(char*)"2");
		xmb[2].member[xmb[2].size-1].option_selected=xmb_sparks;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"XMMB Game Poster", (char*)"Changes display setting for game poster images in XMMB mode.",	(char*)"xmb_game_bg");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Enable",						(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=xmb_game_bg;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"XMMB Game Cover", (char*)"Changes display setting for game cover images in XMMB display mode.",	(char*)"xmb_cover");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Enable",						(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=xmb_cover;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"XMMB Info Pop-up", (char*)"Changes display setting for information pop-up box.",	(char*)"xmb_popup");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Enable",						(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=xmb_popup;


		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Parental Control PIN Code", (char*)"Sets the parental control level PIN code.",	(char*)"parental_pass");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 1, (char*)"****",						(char*)parental_pass);
		xmb[2].member[xmb[2].size-1].option_selected=0;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Parental Control Level", (char*)"Sets the parental control level for rated titles.",	(char*)"parental_level");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 1",						(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 2",						(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 3",						(char*)"3");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 4",						(char*)"4");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 5",						(char*)"5");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 6",						(char*)"6");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 7",						(char*)"7");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 8",						(char*)"8");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 9",						(char*)"9");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 10",						(char*)"10");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Level 11",						(char*)"11");
		xmb[2].member[xmb[2].size-1].option_selected=parental_level;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Function Control", (char*)"Adjusts which functions will be enabled or disabled.",	(char*)"disable_options");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Enable Copy and Delete",	(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable Delete",			(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable Copy",				(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable Copy and Delete",	(char*)"3");
		xmb[2].member[xmb[2].size-1].option_selected=disable_options;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"File Manager Access", (char*)"Sets whether to restrict access to File Manager.",	(char*)"lock_fileman");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Allow",	(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Deny",			(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=lock_fileman;
		xmb[2].member[xmb[2].size-1].icon=xmb_icon_desk;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Verify USB Games", (char*)"Sets whether to check titles on USB for compatibility.",	(char*)"verify_data");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Auto",						(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Always",					(char*)"2");
		xmb[2].member[xmb[2].size-1].option_selected=verify_data;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Scan for Homebrew Applications", (char*)"Sets whether to scan for applications with RELOAD.SELF boot file.",	(char*)"scan_for_apps");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Yes",						(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=scan_for_apps;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Scan for AVCHD\xE2\x84\xA2 and Blu-ray\xE2\x84\xA2", (char*)"Sets whether to scan USB devices for AVCHD\xE2\x84\xA2 and Blu-ray\xE2\x84\xA2 content.",	(char*)"scan_avchd");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No",					(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Yes",					(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=scan_avchd;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Expand Contents of AVCHD\xE2\x84\xA2", (char*)"Sets whether to show one entry per title or all AVCHD\xE2\x84\xA2 playlists.",	(char*)"expand_avchd");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Do not expand",			(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Expand",					(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=expand_avchd;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Hide BD-ROM Disc from Game List", (char*)"Sets appearance of BD-ROM disc entry in the game list.", (char*)"hide_bd");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No",	(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Yes",	(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=hide_bd;
		xmb[2].member[xmb[2].size-1].icon=xmb_icon_blu;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Content Filter", (char*)"Changes default content filter (key shortcut SELECT+R1).",	(char*)"display_mode");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No filtering",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"PS3\xE2\x84\xA2 titles only",		(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"AVCHD\xE2\x84\xA2 titles only",		(char*)"2");
		xmb[2].member[xmb[2].size-1].option_selected=display_mode;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Clean Activity Logs", (char*)"Adjusts whether to remove push list and boot history.", (char*)"clear_activity_logs");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No",		(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Yes",	(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=clear_activity_logs;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Date Format", (char*)"Sets the order of display for year, month and day.", (char*)"date_format");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"DD/MM/YYYY",	(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"MM/DD/YYYY",	(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"YYYY/MM/DD",	(char*)"2");
		xmb[2].member[xmb[2].size-1].option_selected=date_format;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Time Format", (char*)"Sets the time display to either a 12-hour or 24-hour clock.", (char*)"time_format");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"12-Hour Clock",	(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"24-Hour Clock",	(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=time_format;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Button Assignment", (char*)"Sets which buttons are used for Accept/Enter and Cancel/Back.", (char*)"confirm_with_x");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Circle is [Accept]",	(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Cross is [Accept]",	(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=confirm_with_x;


		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Default Display Mode", (char*)"Sets default startup display mode. Switch modes with L1/R1.",	(char*)"full_png");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Game list (plain)",					(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"4x2 game list",						(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Game list (poster)",					(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Game list (user background)",			(char*)"3");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Slide cover flow",						(char*)"4");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"File Manager mode",					(char*)"5");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Box-art",								(char*)"6");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"8x4 game list",						(char*)"7");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"XMMB (XMB\xE2\x84\xA2 clone)",			(char*)"8");
		xmb[2].member[xmb[2].size-1].option_selected=initial_cover_mode;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Display Mode Lock", (char*)"Locks multiMAN to pre-selected display mode.",	(char*)"lock_display_mode");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",								(char*)"-1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Game list (plain)",					(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"4x2 game list",						(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Game list (poster)",					(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Game list (user background)",			(char*)"3");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Slide cover flow",						(char*)"4");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"File Manager mode",					(char*)"5");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Box-art",								(char*)"6");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"8x4 game list",						(char*)"7");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"XMMB (XMB\xE2\x84\xA2 clone)",			(char*)"8");
		xmb[2].member[xmb[2].size-1].option_selected=lock_display_mode+1;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Font Preference", (char*)"Sets default font (key shortcut R3).",	(char*)"user_font");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Default",								(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"New Rodin",							(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Rodin Latin",							(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Rodin Latin 2",						(char*)"3");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Matisse",								(char*)"4");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"User #1 (font0)",						(char*)"5");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"User #2 (font1)",						(char*)"6");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"User #3 (font2)",						(char*)"7");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"User #4 (font3)",						(char*)"8");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"User #5 (font4)",						(char*)"9");
		xmb[2].member[xmb[2].size-1].option_selected=user_font;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Theme Audio", (char*)"Sets whether to play theme music in the background.",	(char*)"theme_sound");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Enable",						(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=theme_sound;


		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"TV Overscan", (char*)"Sets TV overscan zone in percents (key shortcut SELECT+L2/R2).",	(char*)"overscan");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"1%",							(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"2%",							(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"3%",							(char*)"3");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"4%",							(char*)"4");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"5%",							(char*)"5");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"6%",							(char*)"6");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"7%",							(char*)"7");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"8%",							(char*)"8");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"9%",							(char*)"9");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"10%",							(char*)"10");
		if(overscan<0.00f) overscan=0.00f;
		if(overscan>0.10f) overscan=0.10f;
		xmb[2].member[xmb[2].size-1].option_selected=(u8) ((float)overscan * 100.f);
		xmb[2].member[xmb[2].size-1].icon=xmb_icon_ss;

		/*add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Restore Last Mode", (char*)"Sets whether to restore some visual settings upon launch.",	(char*)"load_last_state");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Yes",						(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=load_last_state;*/

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Title Name Appearance", (char*)"Changes size and appearance of title names and paths.",	(char*)"showdir");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Large size title",					(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Title and path",					(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Medium size title",				(char*)"2");
		xmb[2].member[xmb[2].size-1].option_selected=dir_mode;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Title Details", (char*)"Adjusts level of displayed details for selected display modes.",	(char*)"game_details");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Title Only",					(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Title and ID",					(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Full",							(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"None",							(char*)"3");
		xmb[2].member[xmb[2].size-1].option_selected=game_details;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Animation", (char*)"Adjusts animation options for some display modes.",		(char*)"animation");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable all animations",	(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable background slide",	(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable icon animation",	(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"All animations On",		(char*)"3");
		xmb[2].member[xmb[2].size-1].option_selected=animation;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Poster Overlay", (char*)"Sets whether to show poster in [Game list (poster)] display mode.",	(char*)"game_bg_overlay");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Yes",						(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=game_bg_overlay;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Poster and Cover Alteration", (char*)"Sets whether to grayscale game poster and cover when required.",	(char*)"gray_poster");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Never",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Auto",						(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=gray_poster;


		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Progress Bar", (char*)"Sets whether to show progress bar during copy operations.",	(char*)"progress_bar");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"No",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Yes",						(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=progress_bar;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Inactivity Timeout", (char*)"Dim and hide title names after specified amount of time.",	(char*)"dim_titles");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"1 sec",						(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"2 sec",						(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"3 sec",						(char*)"3");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"4 sec",						(char*)"4");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"5 sec",						(char*)"5");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"6 sec",						(char*)"6");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"7 sec",						(char*)"7");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"8 sec",						(char*)"8");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"9 sec",						(char*)"9");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"10 sec",						(char*)"10");
		if(dim_setting<0) dim_setting=0;
		if(dim_setting>10) dim_setting=10;
		xmb[2].member[xmb[2].size-1].option_selected=dim_setting;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Screensaver", (char*)"Turn on screensaver after specified amount of time.",	(char*)"ss_timeout");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"1 min",						(char*)"1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"2 min",						(char*)"2");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"3 min",						(char*)"3");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"4 min",						(char*)"4");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"5 min",						(char*)"5");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"6 min",						(char*)"6");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"7 min",						(char*)"7");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"8 min",						(char*)"8");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"9 min",						(char*)"9");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"10 min",						(char*)"10");
		xmb[2].member[xmb[2].size-1].option_selected=ss_timeout;
		xmb[2].member[xmb[2].size-1].icon=xmb_icon_ss;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Mouse Sensitivity (X)", (char*)"Sets analogue sticks horizontal sensitivity (dead zone).",	(char*)"deadzone_x");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"10%",							(char*)"10");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"20%",							(char*)"20");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"30%",							(char*)"30");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"40%",							(char*)"40");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"50%",							(char*)"50");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"60%",							(char*)"60");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"70%",							(char*)"70");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"80%",							(char*)"80");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"90%",							(char*)"90");
		if(xDZ>90) xDZ=90;
		xmb[2].member[xmb[2].size-1].option_selected=(int) (xDZ/10);

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Mouse Sensitivity (Y)", (char*)"Sets analogue sticks vertical sensitivity (dead zone).",	(char*)"deadzone_y");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",						(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"10%",							(char*)"10");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"20%",							(char*)"20");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"30%",							(char*)"30");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"40%",							(char*)"40");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"50%",							(char*)"50");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"60%",							(char*)"60");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"70%",							(char*)"70");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"80%",							(char*)"80");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"90%",							(char*)"90");
		if(yDZ>90) yDZ=90;
		xmb[2].member[xmb[2].size-1].option_selected=(int) (yDZ/10);

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"Cache Partition", (char*)"Enable or disable 2GB temporary partition (/dev_hdd1).",	(char*)"mount_hdd1");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Disable",					(char*)"0");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Enable",					(char*)"1");
		xmb[2].member[xmb[2].size-1].option_selected=mount_hdd1;

		add_xmb_option(xmb[2].member, &xmb[2].size, (char*)"BD-ROM Emulator", (char*)"Changes emulator type. System restart required to apply changes. ",	(char*)"bd_emulator");
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"None",			(char*)"0");
		if(bdemu2_present)
			add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Hermes",		(char*)"1");
		else
			add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"(Unavailable)",		(char*)"1");
		if(c_firmware==3.55f)
		add_xmb_suboption(xmb[2].member[xmb[2].size-1].option, &xmb[2].member[xmb[2].size-1].option_size, 0, (char*)"Standard",		(char*)"2");
		if(c_firmware==3.41f and bd_emulator>1) bd_emulator=1;
		xmb[2].member[xmb[2].size-1].option_selected=bd_emulator;

}

void init_xmb_icons(t_menu_list *list, int max, int sel)
{
		seconds_clock=0;
		xmb_legend_drawn=0;
		parental_pin_entered=0;
		xmb_sublevel=0;
		xmb_bg_show=0;
		xmb_bg_counter=200;

		load_texture(text_FMS, xmbicons, 128);
		load_texture(text_FMS+(33*65536), blankBG, 320);
		load_texture(text_FMS+(37*65536), xmbdevs, 64);
//		mip_texture( text_FMS+(31*65536), text_bmp, 320, 178, -2); // -> 216x183
		mip_texture( xmb_icon_star_small, xmb_icon_star, 128, 128, -4);
		load_texture(xmb_icon_retro, xmbicons2, 128);


		memset(text_bmp, 0, 8294400);
		load_texture(text_bmp, xmbbg, 1920);

		reset_xmb(0);

		u8 *g_icon=xmb[0].data;

		for(int n=0; n<MAX_XMB_TEXTS; n++)
		{
			xmb_txt_buf[n].used=0;
			xmb_txt_buf[n].data=text_bmpUPSR+(n*XMB_TEXT_WIDTH*XMB_TEXT_HEIGHT*4);
		}
		xmb_txt_buf_max=0;

		for(int n=0; n<MAX_XMB_THUMBS; n++)
		{
			xmb_icon_buf[n].used=-1;
			xmb_icon_buf[n].column=0;
			xmb_icon_buf[n].data=text_bmpUBG+(n*XMB_THUMB_WIDTH*XMB_THUMB_HEIGHT*4);
		}
		xmb_icon_buf_max=0;

		free_all_buffers();

		sprintf(xmb[2].name, "Settings");
		xmb[2].first=0;	
		xmb[2].size=0;

		add_settings_column();
		xmb[2].first=xmb_settings_sel;

		sprintf(xmb[1].name, "multiMAN"); xmb[1].first=1; xmb[1].size=0;
		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"Update", (char*)"Check for multiMAN updates",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_update, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
		if(lock_fileman)
		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"File Manager (Disabled)", (char*)"Manage files and folders",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_desk, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
		else
		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"File Manager", (char*)"Manage files and folders",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_desk, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"Refresh", (char*)"Scan all connected devices for supported content",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb[0].data, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"PFS Driver", (char*)"Toggle between FAT32 and NTFS driver",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_hdd, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);

		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"Screensaver", (char*)"Turn on screensaver mode",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_ss, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);

		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"Themes", (char*)"Change multiMAN appearance",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_theme, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);

		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"Help", (char*)"Start helpMMe application",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_help, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"Restart", (char*)"Close and restart multiMAN",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_restart, 320, 176, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
		add_xmb_member(xmb[1].member, &xmb[1].size, (char*)"Quit", (char*)"Quit multiMAN and return to XMB\xE2\x84\xA2 screen",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_quit, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);

		sprintf(xmb[9].name, "Web"); xmb[9].first=0; xmb[9].size=0;
		add_xmb_member(xmb[9].member, &xmb[9].size, (char*)"Download PS3\xE2\x84\xA2 Demos and Utilities", (char*)"Browse PSX Store Website for rich content for your Playstation\xC2\xAE\x33 system",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_globe, 128, 128, /*f_path*/(char*)"http://www.psxstore.com/", /*i_path*/(char*)"/", 0, 0);
		add_xmb_member(xmb[9].member, &xmb[9].size, (char*)"Download Themes", (char*)"Check for new downloadable themes",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_theme, 128, 128, /*f_path*/(char*)"http://ps3.spiffy360.com/themes.php?category=3", /*i_path*/(char*)"/", 0, 0);
		add_xmb_member(xmb[9].member, &xmb[9].size, (char*)"Visit multiMAN Forum", (char*)"Browse psx-scene thread for multiMAN discussions",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb[1].data, 128, 128, /*f_path*/(char*)"http://psx-scene.com/forums/f187/multiman-multifunctional-tool-your-ps3-game-manager-file-manager-ftp-avchd-bdmv-72826/", /*i_path*/(char*)"/", 0, 0);
		add_xmb_member(xmb[9].member, &xmb[9].size, (char*)"View Online User Guide", (char*)"Browse to GBATemp website for beginner's guide to multiMAN",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb[9].data, 128, 128, /*f_path*/(char*)"http://gbatemp.net/t291170-multiman-beginner-s-guide", /*i_path*/(char*)"/", 0, 0);
		add_xmb_member(xmb[9].member, &xmb[9].size, (char*)"Support multiMAN Development", (char*)"Find how to contribute to multiMAN development",
				/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_help, 128, 128, /*f_path*/(char*)"http://www.google.com/search?q=donate+for+multiMAN", /*i_path*/(char*)"/", 0, 0);

		// Retro columm
		sprintf(xmb[8].name, "Retro"); 
		if(!xmb[8].init)
		{
			xmb[8].first=0; xmb[8].size=0;
		}

		//video column
		sprintf(xmb[5].name, "Video");
		if(!xmb[5].init)
		{
			xmb[5].first=0;	
			xmb[5].size=0;
			add_xmb_member(xmb[5].member, &xmb[5].size, (char*)"Link Video Library to Showtime", (char*)"Make XMB\xE2\x84\xA2 video files available to Showtime",
					/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_showtime, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
			add_xmb_member(xmb[5].member, &xmb[5].size, (char*)"Start Showtime Media Center", (char*)"Launch Showtime to play movies and listen to music",
					/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb_icon_showtime, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
		}


		sprintf(xmb[4].name, "Music");
		sprintf(xmb[3].name, "Photo");


		//game column
		sprintf(xmb[6].name, "Game");
		if(!xmb[6].init)
		{
			xmb[6].first=0;	
			xmb[6].size=0;
			add_xmb_member(xmb[6].member, &xmb[6].size, (char*)"Refresh", (char*)"Scan all connected devices and refresh game list",
					/*type*/6, /*status*/2, /*game_id*/-1, /*icon*/xmb[0].data, 128, 128, /*f_path*/(char*)"/", /*i_path*/(char*)"/", 0, 0);
		}

		//game column
		sprintf(xmb[7].name, "Favorites");
		if(!xmb[7].init)
		{
			xmb[7].first=0;	
			xmb[7].size=0;
		}


		int toff=0;
		int first_sort=0;
		char icon_path[512];
		u8	g_status;
		char t_ip[512];


		draw_xmb_icon_text(xmb_icon);
		//if(xmb_icon==6 || xmb_icon==0) is_game_loading=1;//draw_xmb_bare(6, 1, 1, 0);

		if(!xmb[6].init || !xmb[5].init  || !xmb[7].init)
		{
			for(int m=0; m<max; m++)
			{
				if(!strcmp(list[m].content, "PS3"))
				{
					if(!xmb[6].init)
					{
						if(m==sel) xmb[6].first=xmb[6].size;
						toff=0;
						if(list[m].title[0]=='_') toff=1;
						if(strstr(list[m].path, "/dev_bdvd")==NULL) g_icon=xmb[0].data;
						else {g_icon=xmb_icon_blu; first_sort=1;}
						u16 g_iconw=320; u16 g_iconh=176;
						g_status=0;

		//				sprintf(icon_path, "%s/%s.JPG", covers_dir, list[m].title_id);
		//				if(stat(icon_path, &s3)>=0) {g_iconw=260; g_iconh=300; goto add_mem;}
		//				sprintf(icon_path, "%s/%s.PNG", covers_dir, list[m].title_id);
		//				if(stat(icon_path, &s3)>=0) {g_iconw=260; g_iconh=300; goto add_mem;}

		//				if(strstr(list[m].path, "/pvd_usb")!=NULL)
							sprintf(icon_path, "%s/%s_320.PNG", cache_dir, list[m].title_id); 
		//				else
		//				sprintf(icon_path, "%s/PS3_GAME/ICON0.PNG", list[m].path); 
						if(stat(icon_path, &s3)>=0) {g_iconw=320; g_iconh=176; goto add_mem;}
						sprintf(icon_path, "%s/ICON0.PNG", list[m].path);
						if(stat(icon_path, &s3)>=0) {g_iconw=320; g_iconh=176; goto add_mem;}
						else {g_status=2; g_iconw=128; g_iconh=128;}
		add_mem:
						if(list[m].title[0]=='_') list[m].split=1;
						add_xmb_member(xmb[6].member, &xmb[6].size, list[m].title+toff, list[m].path,
						/*type*/1, /*status*/g_status, /*game_id*/m, /*icon*/g_icon, g_iconw, g_iconh, /*f_path*/list[m].path, /*i_path*/(char*)icon_path, list[m].user, list[m].split);
						
						if(list[m].user & IS_FAV)
						{
							if(m==sel) xmb[7].first=xmb[7].size;
							add_xmb_member(xmb[7].member, &xmb[7].size, list[m].title+toff, list[m].path,
							/*type*/1, /*status*/g_status, /*game_id*/m, /*icon*/g_icon, g_iconw, g_iconh, /*f_path*/list[m].path, /*i_path*/(char*)icon_path, list[m].user, list[m].split);

						}

		//					add_xmb_member(xmb[7].member, &xmb[7].size, list[m].title+toff, list[m].path,
		//					/*type*/1, /*status*/2, /*game_id*/m, /*icon*/xmb_icon_star, 128, 128, /*f_path*/list[m].path, /*i_path*/(char*)"/", list[m].user, list[m].split);

						sort_xmb_col(xmb[6].member, xmb[6].size, first_sort+1);
						/*
						if(xmb_icon==6 || xmb_icon==0)
						{
							if(xmb[6].size<7)
								draw_xmb_bare(6, 1, 0, 0);
							else
								draw_xmb_bare(6, 1, 1, 0);
						}
						*/
					}
				}
				else if(!strcmp(list[m].content, "AVCHD") || !strcmp(list[m].content, "BDMV") || !strcmp(list[m].content, "DVD"))
				{
					if(!xmb[5].init)
					{
						if(m==sel) xmb[5].first=xmb[5].size;
						toff=0;

						if(list[m].title[0]=='_') toff=1;
						else if(strstr(list[m].title, "[Video] ")!=NULL) toff=8;
						else if(strstr(list[m].title, "[HDD Video] ")!=NULL || strstr(list[m].title, "[DVD Video] ")!=NULL) toff=12;
						sprintf(t_ip, "%s/HDAVCTN/BDMT_O1.jpg", list[m].path);
						if(stat(t_ip, &s3)<0) sprintf(t_ip, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", list[m].path);
						if(stat(t_ip, &s3)<0) sprintf(t_ip, "%s/POSTER.JPG", list[m].path);
						if(stat(t_ip, &s3)<0) sprintf(t_ip, "%s/COVER.JPG", list[m].path);
						if(stat(t_ip, &s3)>=0)
							add_xmb_member(xmb[5].member, &xmb[5].size, list[m].title+toff, list[m].content,
							/*type*/2, /*status*/0, /*game_id*/m, /*icon*/xmb_icon_film, 128, 128, /*f_path*/list[m].path, /*i_path*/(char*) t_ip, 0, 0);
						else
							add_xmb_member(xmb[5].member, &xmb[5].size, list[m].title+toff, list[m].content,
							/*type*/2, /*status*/2, /*game_id*/m, /*icon*/xmb_icon_film, 128, 128, /*f_path*/list[m].path, /*i_path*/(char*)"/", 0, 0);
						//draw_xmb_bare(5, 0, 0);
					}
				}
			}

			//sort_xmb_col(xmb[6].member, xmb[6].size, first_sort+1);
			sort_xmb_col(xmb[7].member, xmb[7].size, 0);
		}

		for(int m=0; m<max; m++)
		{
			if(!strcmp(list[m].content, "PS3"))
			{
				if(m==sel) 
					for(int m2=0; m2<xmb[6].size; m2++)
						if(xmb[6].member[m2].game_id==sel) xmb[6].first=m2;
			}
			else if(!strcmp(list[m].content, "AVCHD") || !strcmp(list[m].content, "BDMV") || !strcmp(list[m].content, "DVD"))
			{
				if(m==sel) 
					for(int m2=0; m2<xmb[5].size; m2++)
						if(xmb[5].member[m2].game_id==sel) xmb[5].first=m2;
			}
		}


		if(xmb[6].size>1 && xmb[6].first==0) xmb[6].first=1;
		xmb[6].init=1;
		xmb[7].init=1;

		is_game_loading=0;

		if(xmb_icon==5) add_video_column();
		if(xmb_icon==4) add_music_column();
		if(xmb_icon==3) add_photo_column();
		if(xmb_icon==8) add_emulator_column();

/*
	u8 type; //0 unkn, 1 game, 2 video from gamelist, 3 showtime vid, 4 music, 5 photo, 6 function, 7 setting
	u8 status; //0 idle, 1=loading, 2 loaded, 9 inactive
	int game_id; //pointer to menu_list[id]
	char name[128];
	char subname[128];
	int	data; //pointer to text image (xmbtexts) xmb_txt_buf
	u8	*icon; //pointer to icon image
	u16	iconw;
	u16	iconh;
	char file_path[512]; //path to entry file
	char icon_path[512]; //path to entry icon
*/

}

void draw_xmb_clock(u8 *buffer, const int _xmb_icon)
{
		if(_xmb_icon) seconds_clock=0;
		if( (time(NULL)-seconds_clock)<30) goto clock_text;
		if(_xmb_icon) seconds_clock=0; else seconds_clock=time(NULL);

		char xmb_date[32];
		time ( &rawtime ); timeinfo = localtime ( &rawtime );
		//int sec_char=0x2e; if( (timeinfo->tm_sec)&1 ) sec_char=0x3a;
//		if(_xmb_icon==0)
		{
			if(date_format==0)	sprintf(xmb_date, "%d/%d %s:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, tmhour(timeinfo->tm_hour), timeinfo->tm_min); //, timeinfo->tm_sec
			else sprintf(xmb_date,"%d/%d %s:%02d", timeinfo->tm_mon+1, timeinfo->tm_mday, tmhour(timeinfo->tm_hour), timeinfo->tm_min);

//			print_label_ex( 0.501f, 0.02f, 0.9f, 0xc0202020, xmb_date, 1.04f, 0.0f, 2, 6.40f, 36.0f, 1);
			print_label_ex( 0.5f, 0.0f, 0.9f, COL_XMB_CLOCK, xmb_date, 1.04f, 0.0f, 2, 6.40f, 36.0f, 1);
		} 
//		else 
		{
			if(_xmb_icon==-1)
			{
			//	sprintf(xmb_date, "%s", "Scanning, please wait!");
				if(time(NULL)&1)
				{
					set_texture(xmb[0].data, 128, 128);
					display_img(1770, 74, 64, 64, 128, 128, 0.6f, 128, 128);
				}
			}
			if(_xmb_icon>0 && _xmb_icon<MAX_XMB_ICONS)
			{
				set_texture(xmb[_xmb_icon].data, 128, 128);
				display_img(1834, 74, 64, 64, 128, 128, 0.6f, 128, 128);
			}
//			else
//				sprintf(xmb_date, "Loading %s", xmb[_xmb_icon].name);
//			print_label_ex( 0.5f, 0.0f, 0.7f, 0xc0c0c0c0, xmb_date, 1.00f, 0.0f, 2, 6.40f, 36.0f, 1);
		}

		memset(buffer, 0, 36000); flush_ttf(buffer, 300, 30);
clock_text:
		set_texture(buffer, 300, 30);
		display_img(1520, 90, 300, 30, 300, 30, 0.7f, 300, 30);
}

void draw_xmb_legend(const int _xmb_icon)
{
	if(xmb_bg_counter>30 || _xmb_icon==1 || _xmb_icon==9 || c_opacity2<=0x40 || xmb_slide_step!=0 || xmb_slide_step_y!=0 || xmb_popup==0 || key_repeat) return;
	if(!xmb_legend_drawn)
	{
		xmb_legend_drawn=1;
		char xmb_text[32]; xmb_text[0]=0;
//		memset(text_MSG, 0x80, 84000); 
		for(int fsr=0; fsr<84000; fsr+=4) *(uint32_t*) ((uint8_t*)(text_MSG)+fsr)=0x22222280;

		if( (_xmb_icon==6 && xmb[_xmb_icon].first) || _xmb_icon==7) 
		{
			sprintf(xmb_text, (char*) ": Game Settings");
			put_texture_with_alpha_gen( text_MSG, text_DOX+(dox_triangle_x*4 + dox_triangle_y	* dox_width*4), dox_triangle_w,	dox_triangle_h, dox_width, 300, 8, 5);
		}

		if((_xmb_icon>1 &&_xmb_icon<6) || (_xmb_icon==6 && xmb[_xmb_icon].first==0) || _xmb_icon==8) 
		{
			if(_xmb_icon==2 && xmb[_xmb_icon].first>1) sprintf(xmb_text, (char*) ": Change Setting");
			else if(_xmb_icon==2 && xmb[_xmb_icon].first==0) sprintf(xmb_text, (char*) ": View System Information");
			else if(_xmb_icon==2 && xmb[_xmb_icon].first==1) sprintf(xmb_text, (char*) ": Clear Cached Files");
			else if(_xmb_icon==3) sprintf(xmb_text, (char*) ": View Photo");
			else if(_xmb_icon==4) sprintf(xmb_text, (char*) ": Play Music");
			else if(_xmb_icon==5 && xmb[_xmb_icon].first<2) sprintf(xmb_text, (char*) ": Launch Showtime");
			else if(_xmb_icon==5 && xmb[_xmb_icon].first>1) sprintf(xmb_text, (char*) ": Play Video");
			else if(_xmb_icon==6 && xmb[_xmb_icon].first==0) sprintf(xmb_text, (char*) ": Refresh List");
			else if(_xmb_icon==8 && xmb[_xmb_icon].first==0) sprintf(xmb_text, (char*) ": Refresh ROMs");
			else if(_xmb_icon==8 && xmb[_xmb_icon].first) sprintf(xmb_text, (char*) ": Load Game ROM");
			
			put_texture_with_alpha_gen( text_MSG, text_DOX+(dox_cross_x	*4 + dox_cross_y	* dox_width*4), dox_cross_w,	dox_cross_h,	dox_width, 300, 8, 5);
		}

		put_texture_with_alpha_gen( text_MSG, text_DOX+(dox_l1_x*4 + dox_l1_y	* dox_width*4), dox_l1_w,	dox_l1_h, dox_width, 300, 8, 42);
		put_texture_with_alpha_gen( text_MSG, text_DOX+(dox_r1_x*4 + dox_r1_y	* dox_width*4), dox_r1_w,	dox_r1_h, dox_width, 300, 8+dox_l1_w+5, 42);

		print_label_ex( 0.17f, 0.10f, 0.6f, 0xffd0d0d0, xmb_text, 1.00f, 0.0f, 2, 6.40f, 18.0f, 0);
		print_label_ex( 0.475f, 0.55f, 0.6f, 0xffd0d0d0, (char*) ": Switch Display", 1.00f, 0.0f, 2, 6.40f, 18.0f, 0);
		flush_ttf(text_MSG, 300, 70);
	}
	set_texture(text_MSG, 300, 70);
	display_img(1520, 930, 300, 70, 300, 70, -0.1f, 300, 70);
}

void redraw_column_texts(int _xmb_icon)
{
	for(int n=0; n<xmb[_xmb_icon].size; n++) xmb[_xmb_icon].member[n].data=-1;
	if(_xmb_icon==2) //settings
	{
		u8 first=xmb[2].first;
		xmb[2].first=0;	
		xmb[2].size=0;
		add_settings_column();
		xmb[2].first=first;
	}
}

void draw_xmb_icon_text(int _xmb_icon)
{
	xmb_legend_drawn=0;
	//draw column name
	max_ttf_label=0;
//	print_label_ex( 0.504f, 0.04f, 1.0f, 0x101010ff, xmb[_xmb_icon].name, 1.04f, 0.0f, 8, 4.48f, 24.0f, 1);
	print_label_ex( 0.5f, 0.0f, 1.0f, COL_XMB_COLUMN, xmb[_xmb_icon].name, 1.04f, 0.0f, 8, 4.48f, 24.0f, 1);
	memset(xmb_col, 0, 36000);
	flush_ttf(xmb_col, 300, 30);
	for(int n=0; n<xmb[_xmb_icon].size; n++) xmb[_xmb_icon].member[n].data=-1;
}


void draw_stars()
{
	for(int n=0; n<MAX_STARS; n++)
	{
		draw_square(((float)stars[n].x/1920.0f-0.5f)*2.0f, (0.5f-(float)stars[n].y/1080.0f)*2.0f, (stars[n].size/1920.f), (stars[n].size/1080.f), 0.0f, ( (XMB_SPARK_COLOR&0xffffff00) | stars[n].bri));
		int move_star= rndv(10);
		if(move_star>4) 
			{stars[n].x++; } 
		else 
			{stars[n].x--; }

		if(move_star>3) 
			{stars[n].y++; } 
		if(move_star==2) 
			{stars[n].y--; }

		if(move_star>6) stars[n].bri-=4;
		if(stars[n].x>1919 || stars[n].y>1079 || stars[n].x<1 || stars[n].y<1 || stars[n].bri<4)
		{
			stars[n].x=rndv(1920);
			if(cover_mode==8) stars[n].y=rndv(360)+360;
			else stars[n].y=rndv(1080);
			stars[n].bri=rndv(222);
			stars[n].size=rndv(XMB_SPARK_SIZE)+1;
		}
	}
}

void launch_web_browser(char *start_page)
{ // start web browser

//if(MEMORY_CONTAINER_SIZE_ACTIVE < MEMORY_CONTAINER_SIZE_WEB)
{
//	sys_memory_container_destroy( memory_container );
	int ret = sys_memory_container_create( &memory_container_web, MEMORY_CONTAINER_SIZE_WEB);
	if(ret!=CELL_OK) {
//		sprintf(string1, "Failed to create new memory container!");dialog_ret=0;		cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
//		ret = sys_memory_container_create( &memory_container, MEMORY_CONTAINER_SIZE_ACTIVE);
		goto err_web;
	}
//	else
//		MEMORY_CONTAINER_SIZE_ACTIVE=MEMORY_CONTAINER_SIZE_WEB;
}

	cellWebBrowserConfig2(&config_full, CELL_WEBBROWSER_MK_VER(2, 0)); // initialize config
	cellWebBrowserConfigSetFunction2(&config_full, CELL_WEBBROWSER_FUNCTION2_MOUSE | CELL_WEBBROWSER_FUNCTION2_URL_INPUT | CELL_WEBBROWSER_FUNCTION2_SETTING | CELL_WEBBROWSER_FUNCTION2_BOOKMARK);
	cellWebBrowserConfigSetTabCount2(&config_full, 2);
	cellWebBrowserConfigSetHeapSize2(&config_full, 36 * 1024 * 1024); //2tabs 34mb and 62 container works
	#if (CELL_SDK_VERSION<=0x210001)
	cellWebBrowserConfigSetViewCondition2(&config_full, CELL_WEBBROWSER_VIEWCOND2_NO_FULL_SCREEN);	
	#else
	cellWebBrowserConfigSetViewCondition2(&config_full, CELL_WEBBROWSER_VIEWCOND2_OVERFLOW_AUTO | CELL_WEBBROWSER_VIEWCOND2_TRANSPARENT | CELL_WEBBROWSER_VIEWCOND2_NO_FULL_SCREEN);
	#endif

	cellWebBrowserConfigSetUnknownMIMETypeHook2(&config_full, unknown_mimetype_callback, NULL);
	cellWebBrowserEstimate2(&config_full, &mc_size);

	if((uint32_t)mc_size <= MEMORY_CONTAINER_SIZE_WEB)
	{
		dimc=0; dim=1;c_opacity_delta=-2; c_opacity=0x98; c_opacity2=0x98;
		www_running = 1;
		sprintf(www_info, "%s", "Web browser mode");
		cellWebBrowserInitialize(system_callback, memory_container_web);
		cellWebBrowserCreate2(&config_full, start_page);//?d=MTk1MA
		//screen_saver();
	}
	else
	{
err_web:
		www_running = 0;
		sprintf(www_info, "%s", "Game mode");
//		sprintf(string1, "Browser disabled during Remote Play!\n\nNot enough memory to launch web browser!\n\nRequired memory: %.2f MB (allocated %.2f MB)", (double) mc_size/1024/1024, (double) MEMORY_CONTAINER_SIZE_ACTIVE/1024/1024);dialog_ret=0;		cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
		dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, "Not enough memory to launch web browser!\n\nPlease restart multiMAN and try again.", dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();

	}
}

/****************************************************/
/* MAIN                                             */
/****************************************************/

char bluray_game[64], bluray_id[10];
int bluray_pl=0;

u8 read_pad_info()
{
	u8 to_return=0;
	int skip_t=14;
	if ( ( ( (new_pad & BUTTON_UP) || ( (new_pad & BUTTON_L2) && (cover_mode==1 || cover_mode==7))) && (cover_mode!=4 && cover_mode!=6)) || ((new_pad & BUTTON_LEFT) && (cover_mode==4 || cover_mode==6)) ) {		
		c_opacity_delta=16;	dimc=0; dim=1;
//		old_pad=0; new_pad=0; 
		counter_png=30;
		if(cover_mode==4) is_sliding=-1;
		if(cover_mode==1) skip_t=4; else 
		if(cover_mode==7) skip_t=8; else skip_t=1;
		if(cover_mode==1 && (new_pad & BUTTON_L2)) skip_t=8;
		if(cover_mode==7 && (new_pad & BUTTON_L2)) skip_t=32;
		if(cover_mode!=8)
		{
			game_sel_last=game_sel;
			game_sel-=skip_t;
		}
		new_pad=0;
		if(cover_mode==8 && xmb[xmb_icon].size>1)
		{
			xmb_legend_drawn=0;
			if(xmb[xmb_icon].first==0) {xmb[xmb_icon].first=xmb[xmb_icon].size-1; xmb_bg_show=0; xmb_bg_counter=200; xmb_slide_y=0; xmb_slide_step_y=0;}
			else 
			{
				if(xmb_slide_step_y!=0)
				{
					if(xmb_slide_y >0) { if(xmb[xmb_icon].first>0) xmb[xmb_icon].first-=repeat_counter3; xmb_slide_y=0;}
					if(xmb_slide_y <0) { if(xmb[xmb_icon].first<xmb[xmb_icon].size-1) xmb[xmb_icon].first+=repeat_counter3; xmb_slide_y=0;}
					if(xmb[xmb_icon].first==1 || xmb[xmb_icon].first>=xmb[xmb_icon].size) {repeat_counter1=120; repeat_counter2=8;repeat_counter3=1;repeat_counter3_inc=0.f;}
					if(xmb[xmb_icon].first>=xmb[xmb_icon].size) xmb[xmb_icon].first=xmb[xmb_icon].size-1;
				}
				//else
				xmb_slide_step_y=10;
			}
		}
		//if(cover_mode==8) goto leave_for8;
		if(game_sel<0) { 
			game_sel=max_menu_list-1;	game_sel_last=game_sel; 
			//if(cover_mode==8 && xmb_icon==6) xmb[xmb_icon].first=game_sel;
			//xmb_slide_step_y=0;
			counter_png=0; new_pad=0; if(cover_mode!=8) {to_return=1; goto leave_for8;}}
		if(cover_mode==0 || cover_mode==2) {
			draw_list_text( text_bmp, 1920, 1080, menu_list, max_menu_list, game_sel | (0x10000 * ((menu_list[0].flags & 2048)!=0)), dir_mode, display_mode, cover_mode, c_opacity, 0);
			if( (int) (game_sel/14) != (int) (game_sel_last/14)) {counter_png=0;new_pad=0;{to_return=1; goto leave_for8;}}
		}
		if(cover_mode==1 && game_last_page!=int(game_sel/8)){game_last_page=-1;new_pad=0;{to_return=1; goto leave_for8;}}
		if(cover_mode==7 && game_last_page!=int(game_sel/32)){game_last_page=-1;new_pad=0;{to_return=1; goto leave_for8;}}
	}

	if ( (((new_pad & BUTTON_DOWN) || ( (new_pad & BUTTON_R2) && (cover_mode==1 || cover_mode==7))) && (cover_mode!=4 && cover_mode!=6)) || ((new_pad & BUTTON_RIGHT) && (cover_mode==4 || cover_mode==6)) ) {
//		old_pad=0; new_pad=0; 
		c_opacity_delta=16;	dimc=0; dim=1;
		counter_png=30;
		if(cover_mode==4) is_sliding=1; 
		if(cover_mode==1) skip_t=4; else 
		if(cover_mode==7) skip_t=8; else skip_t=1;
		if(cover_mode==1 && (new_pad & BUTTON_R2)) skip_t=8;
		if(cover_mode==7 && (new_pad & BUTTON_R2)) skip_t=32;
		if(cover_mode!=8)
		{
			game_sel_last=game_sel;
			game_sel+=skip_t;
			if(skip_t>1 && game_sel>=max_menu_list) game_sel=max_menu_list-1;
		}
		new_pad=0;
		if(cover_mode==8 && xmb[xmb_icon].size>1)
		{
			xmb_legend_drawn=0;
			if(xmb[xmb_icon].first==xmb[xmb_icon].size-1) {xmb[xmb_icon].first=0; xmb_bg_show=0; xmb_bg_counter=200; xmb_slide_y=0; xmb_slide_step_y=0;}
			else 
			{
				if(xmb_slide_step_y!=0)
				{
					if(xmb_slide_y >0) { if(xmb[xmb_icon].first>0) xmb[xmb_icon].first-=repeat_counter3; xmb_slide_y=0;}
					if(xmb_slide_y <0) { if(xmb[xmb_icon].first<xmb[xmb_icon].size-1) xmb[xmb_icon].first+=repeat_counter3; xmb_slide_y=0;}
					if(xmb[xmb_icon].first>=xmb[xmb_icon].size-2) {repeat_counter1=120; repeat_counter2=8;repeat_counter3=1;repeat_counter3_inc=0.f;}
				}
				//else
					xmb_slide_step_y=-10;
				if(xmb[xmb_icon].first>=xmb[xmb_icon].size) xmb[xmb_icon].first=xmb[xmb_icon].size-1;

			}
				

		}
//		if(cover_mode==8) goto leave_for8;
		if(game_sel>=max_menu_list) { 
			//if(cover_mode==8 && xmb_icon==6) xmb[xmb_icon].first=0; 
			//xmb_slide_step_y=0;
			game_sel=0; game_sel_last=0; counter_png=0; new_pad=0; if(cover_mode!=8) {to_return=1; goto leave_for8;}
			}

		if(cover_mode==0 || cover_mode==2) {
			if( (int) (game_sel/14) != (int) (game_sel_last/14)) {counter_png=0; new_pad=0; {to_return=1; goto leave_for8;}}
			else draw_list_text( text_bmp, 1920, 1080, menu_list, max_menu_list, game_sel | (0x10000 * ((menu_list[0].flags & 2048)!=0)), dir_mode, display_mode, cover_mode, c_opacity, 0);	
			}
		if(cover_mode==1 && game_last_page!=int(game_sel/8)){game_last_page=-1;new_pad=0;{to_return=1; goto leave_for8;}}
		if(cover_mode==7 && game_last_page!=int(game_sel/32)){game_last_page=-1;new_pad=0;{to_return=1; goto leave_for8;}}
	}

	if ( ((new_pad & BUTTON_LEFT) && (cover_mode!=4 && cover_mode!=6)) || ((new_pad & BUTTON_UP) && (cover_mode==4 || cover_mode==6)) ) { 
//		old_pad=0; new_pad=0; 
		xmb_legend_drawn=0;
		if(cover_mode==8 && xmb_icon>1) 
		{
			if(xmb_slide_step!=0)
			{
				if(xmb_slide >0) {if(xmb_icon>2) xmb_icon--; if(xmb_icon==2) free_all_buffers(); xmb_slide=0;draw_xmb_icon_text(xmb_icon);}
				if(xmb_slide <0) {if(xmb_icon<MAX_XMB_ICONS-2) xmb_icon++; if(xmb_icon==4) free_all_buffers(); xmb_slide=0; draw_xmb_icon_text(xmb_icon);}
				if(xmb_icon!=1) xmb_slide_step=15;
				if(xmb_icon==2) {repeat_counter1=120; repeat_counter2=8;repeat_counter3=1;repeat_counter3_inc=0.f;}
			}
			else
				xmb_slide_step=15;
		}
		if(cover_mode==8) goto leave_for8;
		if(cover_mode==1 || cover_mode==7) skip_t=1; 
		else if(cover_mode==6) skip_t=3;
		else skip_t=14;

		c_opacity_delta=16; dimc=0; dim=1; 
		if(game_sel == 0) { game_sel=max_menu_list-1; game_sel_last=game_sel; counter_png=0; new_pad=0; {to_return=1; goto leave_for8;}} 
		else { 
//			old_pad=0; new_pad=0; 
			game_sel = game_sel-skip_t; game_sel_last=game_sel; 
			if(game_sel<0) {game_sel = 0; game_sel_last=0; counter_png=0; new_pad=0;{to_return=1; goto leave_for8;}} 
		}
		if(cover_mode==1 && game_last_page!=int(game_sel/8)){game_last_page=-1;new_pad=0;{to_return=1; goto leave_for8;}}
		if(cover_mode==7 && game_last_page!=int(game_sel/32)){game_last_page=-1;new_pad=0;{to_return=1; goto leave_for8;}}
	}

	if ( ((new_pad & BUTTON_RIGHT) && (cover_mode!=4 && cover_mode!=6)) || ((new_pad & BUTTON_DOWN) && (cover_mode==4 || cover_mode==6)) ) { 
//		old_pad=0; new_pad=0; 
		xmb_legend_drawn=0;
		if(cover_mode==8 && xmb_icon<MAX_XMB_ICONS-1) 
		{
			if(xmb_slide_step!=0)
			{
				if(xmb_slide >0) {if(xmb_icon>2) xmb_icon--; if(xmb_icon==2) free_all_buffers(); xmb_slide=0; draw_xmb_icon_text(xmb_icon);}
				if(xmb_slide <0) {if(xmb_icon<MAX_XMB_ICONS-2) xmb_icon++; if(xmb_icon==4) free_all_buffers(); xmb_slide=0;draw_xmb_icon_text(xmb_icon);}
				xmb_slide_step=-15;
				if(xmb_icon!=MAX_XMB_ICONS-1) xmb_slide_step=-15;
				if(xmb_icon==MAX_XMB_ICONS-2) {repeat_counter1=120; repeat_counter2=8;repeat_counter3=1;repeat_counter3_inc=0.f;}
			}
			else
				xmb_slide_step=-15;
		}
		if(cover_mode==8) goto leave_for8;
		if(cover_mode==1 || cover_mode==7) skip_t=1; 
		else if(cover_mode==6) skip_t=3;
		else skip_t=14;
		c_opacity_delta=16; dimc=0; dim=1;
		if(game_sel == max_menu_list-1) { game_sel = 0; game_sel_last=game_sel; } 
		else { 
//			old_pad=0; new_pad=0; 
			game_sel = game_sel + skip_t; game_sel_last=game_sel; 
			if(game_sel>=max_menu_list) {game_sel=max_menu_list-1; new_pad=0; game_sel_last=game_sel; counter_png=0; {to_return=1; goto leave_for8;}} 
		} 
		if(cover_mode==1 && game_last_page!=int(game_sel/8)){game_last_page=-1;new_pad=0; {to_return=1; goto leave_for8;}}
		if(cover_mode==7 && game_last_page!=int(game_sel/32)){game_last_page=-1;new_pad=0; {to_return=1; goto leave_for8;}}
	}


		if((old_pad & BUTTON_SELECT) && (new_pad & BUTTON_R2))
		{ 
			c_opacity_delta=16;	dimc=0; dim=1;
			new_pad=0; //state_draw=1;
			overscan+=0.01f; if(overscan>0.10f) overscan=0.10f;
			if(cover_mode==8 && xmb_icon==2) redraw_column_texts(xmb_icon);
			{to_return=1; goto leave_for8;}
		}

		if((old_pad & BUTTON_SELECT) && (new_pad & BUTTON_L2))
		{ 
			c_opacity_delta=16;	dimc=0; dim=1;
			new_pad=0; //old_pad=0; state_draw=1;
			overscan-=0.01f;if(overscan<0.0f) overscan=0.00f;
			if(cover_mode==8 && xmb_icon==2) redraw_column_texts(xmb_icon);
			{to_return=1; goto leave_for8;}
		}


leave_for8:
	return to_return;
}

void draw_whole_xmb(u8 mode)
{
	char filename[1024];
	drawing_xmb=1;
	if(mode)
	{
		pad_read();
		read_pad_info();
		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	}

	if((xmb_icon>1 && xmb_icon<6) || xmb_icon==8) xmb_bg_counter--;
	if(xmb_icon==5 && xmb[5].init==0) add_video_column();// && xmb_bg_counter<100
	else if(xmb_icon==4 && xmb[4].init==0) add_music_column();
	else if(xmb_icon==3 && xmb[3].init==0) add_photo_column();
	else if(xmb_icon==8 && xmb[8].init==0) add_emulator_column();

	if(xmb_slide_step!=0 || xmb_slide_step_y!=0 || (xmb_game_bg==0 && xmb_cover==0)) xmb_bg_show=0;
	if(xmb_sparks==1 || (xmb_sparks==2 && xmb_bg_show==0)) draw_stars();

	if(!xmb_bg_show) set_texture( text_bmp, 1920, 1080); else set_texture( text_FONT, 1920, 1080);
	display_img(0, 0, 1920, 1080, 1920, 1080, 0.9f, 1920, 1080);

/*	set_texture( text_bmp, 1920, 1080);
	display_img(0, 0, 1920, 1080, 1920, 1080, 0.9f, 1920, 1080);
	if(xmb_bg_show) 
	{ 
		set_texture( text_FONT, 1920, 1080);
		display_img(0, 0, 1920, 1080, 1920, 1080, 0.8f, 1920, 1080);
	} */

	draw_xmb_clock(xmb_clock, 0);
	draw_xmb_legend(xmb_icon);

	if(xmb_slide_step!=0) //xmmb sliding horizontally
	{
		xmb_slide+=xmb_slide_step;
		if(xmb_slide==180) xmb_slide_step=2;
		else if(xmb_slide ==-180)  xmb_slide_step=-2;
		else if(xmb_slide >= 200) {xmb_slide_step= 0; if(xmb_icon==3) free_all_buffers(); xmb_icon--; xmb_slide=0; draw_xmb_icon_text(xmb_icon); parental_pin_entered=0;}
		else if(xmb_slide <=-200) {xmb_slide_step= 0; if(xmb_icon==3) free_all_buffers(); xmb_icon++; xmb_slide=0; draw_xmb_icon_text(xmb_icon); parental_pin_entered=0;}

		if(xmb_icon>MAX_XMB_ICONS-1) xmb_icon=MAX_XMB_ICONS-1;
		else if(xmb_icon<1) xmb_icon=1;
		if(xmb_slide_step==0) xmb_bg_counter=200;
	}

	if(xmb_slide_step_y!=0) //xmmb sliding vertically
	{
		xmb_slide_y+=xmb_slide_step_y;
			 if(xmb_slide_y == 40) xmb_slide_step_y = 5;
		else if(xmb_slide_y ==-40) xmb_slide_step_y =-5;
		else if(xmb_slide_y == 80) xmb_slide_step_y = 2;
		else if(xmb_slide_y ==-80) xmb_slide_step_y =-2;
		else if(xmb_slide_y >= 90) {xmb_slide_step_y= 0; xmb_legend_drawn=0; if(xmb[xmb_icon].first>0) xmb[xmb_icon].first--; xmb_slide_y=0;}
		else if(xmb_slide_y <=-90) {xmb_slide_step_y= 0; xmb_legend_drawn=0; if(xmb[xmb_icon].first<xmb[xmb_icon].size-1) xmb[xmb_icon].first++; xmb_slide_y=0;}
		if(xmb_slide_step_y==0) xmb_bg_counter=200;
	}
	if( (xmb_icon==6 || xmb_icon==7) && xmb[xmb_icon].size) // && max_menu_list>0
	{
		if(xmb_bg_counter>0 && !xmb_bg_show) xmb_bg_counter--;
		if(xmb_bg_counter==0 && !xmb_bg_show && xmb_slide_step==0 && xmb_slide_step_y==0 && (xmb_game_bg==1 || xmb_cover==1) && (xmb_icon!=6 || (xmb_icon==6 and xmb[xmb_icon].first)) && !is_game_loading)
		{
			sprintf(filename, "%s/%s_1920.PNG", cache_dir, menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].title_id);
			if(stat(filename, &s3)>=0 && xmb_game_bg==1)	
			{
				//load_png_partial(text_FONT, filename, 1920, 90, 0);
				//load_png_texture(text_FONT, filename, 1920);
				//sprintf(filename, "%s/PS3_GAME/PIC0.PNG", menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].path);
				//if(stat(filename, &s3)>=0) 	load_png_texture(text_FONT+1998640, filename, 1920);
				load_png_texture(text_FONT, filename, 1920);
				//change_opacity(text_FONT, -75, 8294400);
				if(menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].split==1 || menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].title[0]=='_') gray_texture(text_FONT, 1920, 1080, 0);
				//blur_texture(text_FONT, 1920, 1080, 0, 0, 1920, 1080,  70, ((menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].split==1 || menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].title[0]=='_') ? 1 : 0), 1, 1);
			}
			else memcpy(text_FONT, text_bmp, 8294400);

			if(xmb_cover==1)// && xmb_icon==6)
			{
				sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].title_id);
				char cvstr [128];
				if(stat(filename, &s3)>=0) 
					{
						sprintf(cvstr, "%s/CBOX3.PNG", app_usrdir);
						if(stat(cvstr, &s3)>=0){
							load_texture(text_TEMP, cvstr, 349);
							put_texture_with_alpha( text_FONT, text_TEMP, 349, 356, 349, 785, 550, 0, 0);
						}

						load_jpg_texture(text_FONT+4419256, filename, 1920);//(575*1920+814)*4

						sprintf(filename, "%s/GLC.PNG", app_usrdir);
						if(stat(filename, &s3)>=0)
						{
							load_texture(text_TEMP, filename, 260);
							put_texture_with_alpha( text_FONT, text_TEMP, 260, 300, 260, 814, 575, 0, 0);
						}
					}
				else 
				{
					if(menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].cover!=-1 && menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].cover!=1)
					{
						download_cover(menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].title_id, filename);
						menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].cover=1;
					}
					sprintf(filename, "%s/%s.PNG", covers_dir, menu_list[xmb[xmb_icon].member[xmb[xmb_icon].first].game_id].title_id);
					if(stat(filename, &s3)>=0) 
					{
						sprintf(cvstr, "%s/CBOX3.PNG", app_usrdir);
						if(stat(cvstr, &s3)>=0){
							load_png_texture(text_TEMP, cvstr, 349);
							put_texture_with_alpha( text_FONT, text_TEMP, 349, 356, 349, 785, 550, 0, 0);
						}
						load_texture(text_FONT+(575*1920+814)*4, filename, 1920);

						sprintf(filename, "%s/GLC.PNG", app_usrdir);
						if(stat(filename, &s3)>=0)
						{
							load_texture(text_TEMP, filename, 260);
							put_texture_with_alpha( text_FONT, text_TEMP, 260, 300, 260, 814, 575, 0, 0);
						}

					}// else xmb_bg_counter=-1;
				}
			}
//				sprintf(icon_path, "%s/%s.PNG", covers_dir, list[m].title_id);
//				if(stat(icon_path, &s3)>=0) {g_iconw=260; g_iconh=300; goto add_mem;}

				xmb_bg_show=1;
		
		}
	}

	draw_xmb_icons(xmb, xmb_icon, xmb_slide, xmb_slide_y, 0, xmb_sublevel);
	if((xmb_icon==6 || xmb_icon==7 || xmb_icon==5) && xmb[xmb_icon].member[xmb[xmb_icon].first].game_id!=-1) game_sel=xmb[xmb_icon].member[xmb[xmb_icon].first].game_id;
	if(xmb_icon==2) xmb_settings_sel=xmb[2].first;

	if(mode) flip();

	drawing_xmb=0;
}

int main(int argc, char **argv) 
{
	cellSysutilRegisterCallback( 0, sysutil_callback, NULL );
	payloadT[0]='M';
	payloadT[1]=0;

	(void)argc;
//	(void)argv;

	int ret;

	int one_time=1;
	
	ret = cellSysmoduleLoadModule( CELL_SYSMODULE_GCM_SYS );
	if (ret != CELL_OK) sys_process_exit(1);
	else unload_mod|=8;
//	fast_files_mem = memalign(8, sizeof(t_fast_files)*MAX_FAST_FILES);
	host_addr = memalign(0x100000, 0x200000);
	if(cellGcmInit(0x10000, 0x200000, host_addr) != CELL_OK) sys_process_exit(1);
	if(initDisplay()!=0) sys_process_exit(1);
	initShader();
	setDrawEnv();
	if(setRenderObject()) sys_process_exit(1);
	ret = cellPadInit(8);
	setRenderTarget();
	initFont();

	app_path[0]='B';
	app_path[1]='L';
	app_path[2]='E';
	app_path[3]='S';
	app_path[4]='8';
	app_path[5]='0';
	app_path[6]='6';
	app_path[7]='0';
	app_path[8]='8';
	app_path[9]=0;

		struct stat s2;
		if(!strncmp( argv[0], "/dev_hdd0/game/", 15))
			{
			char *s;
			int n=0;
			s= ((char *) argv[0])+15;
			while(*s!=0 && *s!='/' && n<32) {app_path[n]=*s++; n++;} app_path[n]=0;
			}

//	sprintf(disclaimer,  "/dev_hdd0/game/%s/DISCLM.BIN",app_path);
//	remove(disclaimer);
//	if(strstr(argv[0], "/RELOAD.SELF")!=NULL) is_reloaded=1; else 
	is_reloaded=0;

	struct stat s4;
	if(strstr(argv[0],"/dev_hdd0/game/")==NULL)
	{
		sprintf(app_homedir, "/dev_hdd0/game/%s",app_path);
		mkdir(app_homedir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); 
		sprintf(app_usrdir, "/dev_hdd0/game/%s/USRDIR",app_path);
		mkdir(app_usrdir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); 
	}
	else
		sprintf(app_usrdir, "/dev_hdd0/game/%s/USRDIR",app_path);

	//sprintf(mmbin, "%s/RELOAD/PS3_GAME/USRDIR/EBOOT.BIN", app_usrdir);

	char string1x[512], string2x[512], string3x[512], string1[1024], filename[1024];

	sprintf(disclaimer,  "%s/DISCLM.BIN", app_usrdir);
	sprintf(string1  ,  "%s/DISACC.BIN", app_usrdir);

	cellMsgDialogAbort();



	if(stat(disclaimer, &s4)<0 || stat(string1, &s4)>=0) 
	{
			
			sprintf(string1x, "multiMAN (referred hereafter as \"software\"), its author, partners, and associates do not condone piracy. multiMAN is a hobby project, distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\nDo you accept this binding agreement (1/3)?");
			dialog_ret=0; cellMsgDialogOpen2( type_dialog_yes_no, string1x, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog(); if(dialog_ret!=1) sys_process_exit(1);
			
			sprintf(string2x, "The software is intended solely for educational and testing purposes, and while it may allow the user to create copies of legitimately acquired and/or owned content, it is required that such user actions must comply with local, federal and country legislation.\n\nFurthermore, the author of this software, its partners and associates shall assume NO responsibility, legal or otherwise implied, for any misuse of, or for any loss that may occur while using multiMAN.\n\nDo you accept this binding agreement (2/3)?");
			dialog_ret=0; cellMsgDialogOpen2( type_dialog_yes_no, string2x, dialog_fun1, (void*)0x0000aaaa, NULL ); wait_dialog();	if(dialog_ret!=1) sys_process_exit(1);

			sprintf(string3x, "You are solely responsible for complying with the applicable laws in your country and you must cease using this software should your actions during multiMAN operation lead to or may lead to infringement or violation of the rights of the respective content copyright holders.\n\nmultiMAN is not licensed, approved or endorsed by \"Sony Computer Entertainment, Inc.\" (SCEI) or any other party.\n\nDo you understand and accept this binding agreement?");
			dialog_ret=0; cellMsgDialogOpen2( type_dialog_yes_no, string3x, dialog_fun1, (void*)0x0000aaaa, NULL ); wait_dialog();	if(dialog_ret!=1) sys_process_exit(1);

			FILE *fpA;
			fpA = fopen ( disclaimer, "w" );
			fputs ( "USER AGREED TO DISCLAIMER: ",  fpA );fputs ( string1x,  fpA );fputs ( string2x,  fpA ); fputs ( string3x,  fpA );
			fclose( fpA);
			remove(string1);
	}

	sprintf(string1x, "%s/RELOADED.BIN", app_usrdir);
	if(stat(string1x, &s3)>=0) is_reloaded=1;
	else
	{
		FILE *fpA;	
		fpA = fopen ( string1x, "w" );	fputs ( "multiMAN was reloaded",  fpA );fputs ( string1x,  fpA );
		fclose( fpA);
	}

	sprintf(current_version_NULL, "%s", current_version);
	sys_spu_initialize(6, 0);
	ret = load_modules();
	if(ret!=CELL_OK && ret!=0) sys_process_exit(1);
	cellSysutilEnableBgmPlayback();


//	xmb=(xmb_def*)vm;


//	flip();

	u32 buf_align= (1920 * 1080 * 4);
	u32 frame_buf_size = (buf_align * 9)+4976640;// for text_bmpS 320x320x12 -> 1920*648 * 4
	frame_buf_size = ( frame_buf_size + 0xfffff ) & ( ~0xfffff );

	text_bmp = (u8 *) memalign(0x100000, frame_buf_size);
	if(png_out_mapmem( text_bmp, frame_buf_size)) exit(-1);

	text_bmpUBG	= text_bmp + buf_align * 1; //4 x 1920x1080
	text_bmpUPSR= text_bmp + buf_align * 5; //2 x 1920x1080
	text_FONT	= text_bmp + buf_align * 7;

	text_FMS	= text_bmp + buf_align * 8;
	text_bmpS	= text_bmp + buf_align * 9;

	//pData = (long)text_bmp + buf_align * 8;//(long)memalign(128, _mp3_buffer*1024*1024);



//	u32 frame_buf_size4 = 1920 * 540 * 4;
//	frame_buf_size4 = ( frame_buf_size4 + 0xfffff ) & ( ~0xfffff );
//	text_FMS= (u8 *) memalign(0x100000, frame_buf_size4); 
//	if(png_out_mapmem( text_FMS, frame_buf_size4)) exit(-1);

	xmb_icon_globe	=	text_FMS+( 9*65536);
	xmb_icon_help	=	text_FMS+(10*65536);
	xmb_icon_quit	=	text_FMS+(11*65536);
	xmb_icon_star	=	text_FMS+(12*65536);
	xmb_icon_desk	=	text_FMS+(13*65536);
	xmb_icon_hdd	=	text_FMS+(14*65536);
	xmb_icon_blu	=	text_FMS+(15*65536);
	xmb_icon_tool	=	text_FMS+(16*65536);
	xmb_icon_note	=	text_FMS+(17*65536);
	xmb_icon_film	=	text_FMS+(18*65536);
	xmb_icon_photo	=	text_FMS+(19*65536);
	xmb_icon_update	=	text_FMS+(20*65536);
	xmb_icon_ss		=	text_FMS+(21*65536);
	xmb_icon_showtime=	text_FMS+(22*65536);
	xmb_icon_theme	=	text_FMS+(23*65536);
//====================================================
	xmb_icon_star_small	=	text_FMS+(24*65536);//+(16384*0)//64*64*4
//	xmb_icon_????_small	=	text_FMS+(24*65536)+(16384*1);
	xmb_col			=	text_FMS+(31*65536);
	xmb_clock		=	text_FMS+(32*65536);
	xmb_icon_restart=	text_FMS+(33*65536);

	xmb_icon_dev	=	text_FMS+(37*65536);

	text_DOX		=	text_FMS+(45*65536);
	text_MSG		=	text_FMS+(49*65536);

	xmb_icon_retro	=	text_FMS+(51*65536);
	xmb_icon_ftp	=	text_FMS+(52*65536);
	xmb_icon_folder	=	text_FMS+(53*65536);

//	text_???		=	text_FMS+(51*65536);

//	u32 frame_buf_size3 = buf_align * 12;
//	frame_buf_size3 = ( frame_buf_size3 + 0xfffff ) & ( ~0xfffff );
//	text_bmpS = (u8 *) memalign(0x100000, frame_buf_size3);
//	if(png_out_mapmem( text_bmpS, frame_buf_size3)) exit(-1);


	u32 buf_align2= (320 * 320 * 4);
	text_HDD   = text_bmpS + buf_align2 * 1;
	text_USB   = text_bmpS + buf_align2 * 2;
	text_BLU_1 = text_bmpS + buf_align2 * 3;
	text_OFF_2 = text_bmpS + buf_align2 * 4;
	text_CFC_3 = text_bmpS + buf_align2 * 5;
	text_SDC_4 = text_bmpS + buf_align2 * 6;
	text_MSC_5 = text_bmpS + buf_align2 * 7;
	text_NET_6 = text_bmpS + buf_align2 * 8;
	text_bmpIC = text_bmpS + buf_align2 * 9;

	text_TEMP  = text_bmpS + buf_align2 * 10; //+11

//	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x000000ff);
//	cellDbgFontPrintf( 0.30f, 0.85f, 1.0f, 0x90909090, "multiMAN is preparing environment...");
//	cellDbgFontDrawGcm(); setRenderColor();	
//	flip();

	sprintf(avchdBG, "%s/BOOT.PNG",app_usrdir); 
	load_texture(text_bmpUBG, avchdBG, 1920);
	sprintf(string1, "%s/GLO.PNG", app_usrdir);
	if(stat(string1, &s3)>=0) 
	{
		load_texture(text_bmpUPSR, string1, 1920);
		put_texture_with_alpha( text_bmpUBG, text_bmpUPSR, 1920, 1080, 1920, 0, 0, 0, 0);
	}

	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	set_texture( text_bmpUBG, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
	flip();

	MEMORY_CONTAINER_SIZE_ACTIVE = MEMORY_CONTAINER_SIZE;
	ret = sys_memory_container_create( &memory_container, MEMORY_CONTAINER_SIZE_ACTIVE);
	if(ret!=CELL_OK) 
	{
		MEMORY_CONTAINER_SIZE_ACTIVE = MEMORY_CONTAINER_SIZE - (2 * 1024 * 1024);
		ret = sys_memory_container_create( &memory_container, MEMORY_CONTAINER_SIZE_ACTIVE);
	}
	if(ret!=CELL_OK) sys_process_exit(1);

	//cellFsChmod(argv[0], 0666);
	url_base[ 0]=0x68; url_base[ 1]=0x74; url_base[ 2]=0x74; url_base[ 3]=0x70;
	url_base[ 4]=0x3a; url_base[ 5]=0x2f; url_base[ 6]=0x2f; url_base[ 7]=0x73;
	url_base[ 8]=0x68; url_base[ 9]=0x61; url_base[10]=0x6a; url_base[11]=0x2e;
	url_base[12]=0x6d; url_base[13]=0x65; url_base[14]=0x2f; url_base[15]=0x64;
	url_base[16]=0x65; url_base[17]=0x61; url_base[18]=0x6e; url_base[19]=0x2f;
	url_base[20]=0x6d; url_base[21]=0x75; url_base[22]=0x6c; url_base[23]=0x74;
	url_base[24]=0x69; url_base[25]=0x6d; url_base[26]=0x61; url_base[27]=0x6e;
	url_base[28]=0x00;

	sys_vm_memory_map(MB(32), MB(vm_real_size), SYS_MEMORY_CONTAINER_ID_INVALID, SYS_MEMORY_PAGE_SIZE_64K, SYS_VM_POLICY_AUTO_RECOMMENDED, &vm);
	sys_vm_touch(vm, MB(vm_real_size));

	pData = (long)(u8*)vm;
	stream = (char *)pData;
	pDataB = pData;

    char last_play_id[10];

	last_play_id[0]=0x42; //B
	last_play_id[1]=0x4C; //L
	last_play_id[2]=0x45; //E
	last_play_id[3]=0x53; //S

	last_play_id[4]=0x38;
	last_play_id[5]=0x30;
	last_play_id[6]=0x36;
	last_play_id[7]=0x31;
	last_play_id[8]=0x30;
	last_play_id[9]=0x00;
	sprintf(last_play, "/dev_hdd0/game/%s/LASTPLAY.BIN", last_play_id);
	sprintf(last_play_dir, "/dev_hdd0/game/%s", last_play_id);
	sprintf(last_play_sfo, "/dev_hdd0/game/%s/PARAM.SFO", last_play_id);

	max_menu_list=0;

	c_firmware = (float) get_system_version();
	if(c_firmware>3.41f && c_firmware<3.42f) c_firmware=3.41f;
	if(c_firmware>3.55f) c_firmware=3.55f;

	mod_mount_table((char*)"nothing", 0); //restore
	for(int n2=0;n2<99;n2++) {
		sprintf(string1, "/dev_usb%03i/PS3_GAME", n2);
		if(stat(string1, &s3)>=0) check_usb_ps3game(string1);
	}

	sprintf(list_file, "%s/LLIST.BIN", app_usrdir);
	sprintf(list_file_state, "%s/LSTAT.BIN", app_usrdir);

	sprintf(snes_self, "%s", "/dev_hdd0/game/SNES90000/USRDIR/RELOAD.SELF");
	sprintf(snes_roms, "%s", "/dev_hdd0/game/SNES90000/USRDIR/roms");

	sprintf(genp_self, "%s", "/dev_hdd0/game/GENP00001/USRDIR/RELOAD.SELF");
	sprintf(genp_roms, "%s", "/dev_hdd0/game/GENP00001/USRDIR/roms");

	sprintf(fceu_self, "%s", "/dev_hdd0/game/FCEU90000/USRDIR/RELOAD.SELF");
	sprintf(fceu_roms, "%s", "/dev_hdd0/game/FCEU90000/USRDIR/roms");

	sprintf(vba_self, "%s", "/dev_hdd0/game/VBAM90000/USRDIR/RELOAD.SELF");
	sprintf(vba_roms, "%s", "/dev_hdd0/game/VBAM90000/USRDIR/roms");

	u32 reload_fdevices=0;

	if(is_reloaded || stat(list_file, &s3)>=0)
	{

//		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
//		cellDbgFontPrintf( 0.28f, 0.85f, 1.0f, 0x90909090, "multiMAN is parsing last state data...");
//		cellDbgFontDrawGcm(); setRenderColor();	flip();
		//is_reloaded=0;

		FILE *flist; 
		flist = fopen(list_file, "rb");
		if ( flist != NULL )
		{
			fread((char*) &string1, 8, 1, flist);
			if(strstr(string1, GAME_LIST_VER)!=NULL)
			{
				fseek(flist, 0, SEEK_END);
				int llist_size=ftell(flist)-8;
				fseek(flist, 8, SEEK_SET);
				fread((char*) &menu_list, llist_size, 1, flist);
				fclose(flist);
				max_menu_list=(int)(llist_size / sizeof(t_menu_list))-1;//sizeof(t_menu_list));

				if(!is_reloaded) {delete_entries(menu_list, &max_menu_list, 1); is_reloaded=1;} else is_reloaded=2;
				sort_entries(menu_list, &max_menu_list );

				int i;
				for(i=0;i<max_menu_list;i++)
				{	if(menu_list[i].cover==1) menu_list[i].cover=0;
					if(stat(menu_list[i].path, &s3)<0) {max_menu_list=0; is_reloaded=0; break;}
					//else {is_reloaded=1; }//forcedevices=0x0001;
				}
			}
			else {fclose(flist);remove(list_file);is_reloaded=0;}
		} else is_reloaded=0;
	}

	if(stat(list_file_state, &s3)>=0)
	{
		FILE *flist; 
		flist = fopen(list_file_state, "rb");
		if ( flist != NULL )
		{
			fread((char*) &string1, 8, 1, flist);
			if(strstr(string1, GAME_STATE_VER)!=NULL)
			{
				fread((char*) &reload_fdevices, sizeof(reload_fdevices), 1, flist);
				fread((char*) &mouseX, sizeof(mouseX), 1, flist);
				fread((char*) &mouseY, sizeof(mouseY), 1, flist);
				fread((char*) &mp3_volume, sizeof(mp3_volume), 1, flist); if(mp3_volume<0.0f) mp3_volume=0.0f;
				fread((char*) &current_left_pane, sizeof(current_left_pane), 1, flist);
				fread((char*) &current_right_pane, sizeof(current_right_pane), 1, flist);
				fread((char*) &xmb_icon_last, sizeof(xmb_icon), 1, flist);
				fread((char*) &xmb_icon_last_first, sizeof(xmb[xmb_icon].first), 1, flist);
				if(is_reloaded==1)forcedevices=0x0001;
			}
			fclose(flist);
		}
	}

	if(true)
	{
		sprintf(list_file_state, "%s/XMBS.ALL", app_usrdir);
		FILE *flist = fopen(list_file_state, "rb");
		if(flist!=NULL)
		{
			fread((char*) &string1, 8, 1, flist);
			if(strstr(string1, XMB_COL_VER)!=NULL)
			{
				fseek(flist, 0, SEEK_END);
				int llist_size=ftell(flist)-8;
				fseek(flist, 8, SEEK_SET);
				fread((char*) &xmb, llist_size, 1, flist);
				fclose(flist);
				free_all_buffers();
				for(int n=0; n<MAX_XMB_TEXTS; n++)
				{
					xmb_txt_buf[n].used=0;
					xmb_txt_buf[n].data=text_bmpUPSR+(n*XMB_TEXT_WIDTH*XMB_TEXT_HEIGHT*4);
				}
				xmb_txt_buf_max=0;
				xmb[5].init=0;
				xmb[6].init=0;
				xmb[7].init=0;
			}
			else
			{
				fclose(flist);
				remove(list_file_state);
			}
		}
	}




//	int reloaded_menu_list=max_menu_list;

		sprintf(string1,  "%s/COLOR.INI", app_usrdir);
		if(stat(string1, &s2)>=0) 
			sprintf(color_ini,  "%s/COLOR.INI", app_usrdir);
		else
			sprintf(color_ini,  "%s/COLOR.BIN", app_usrdir);

		sprintf(options_bin,  "%s/options.bin", app_usrdir);

		sprintf(parental_pass, "0000"); parental_pass[4]=0;

		if(stat(options_bin, &s3)<0) {ftp_on(); ftp_service=1; save_options();}

		int usb_loop;
		for(usb_loop=0;usb_loop<8;usb_loop++)
		{
			sprintf(options_ini, "/dev_usb00%i/options.ini", usb_loop);
			if(stat(options_ini, &s2)<0) sprintf(options_ini, "/dev_usb00%i/options_default.ini", usb_loop);
			if(stat(options_ini, &s2)>=0) break;
		}
		if(stat(options_ini, &s2)>=0) {
			sprintf(string1,  "%s/options.ini", app_usrdir);
			file_copy((char *) options_ini, (char *) string1, 0);
			sprintf(options_ini,  "%s/options.ini", app_usrdir);
		}

		if(stat(options_ini, &s2)<0) {
			sprintf(options_ini, "%s/options.ini",app_usrdir);
			if(stat(options_ini, &s2)<0) sprintf(options_ini, "%s/options_default.ini",app_usrdir);
		}

	sprintf(covers_dir, "%s/covers",app_usrdir);
	mkdir(covers_dir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

	sprintf(covers_retro, "%s/covers_retro",app_usrdir);
	mkdir(covers_retro, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

	sprintf(themes_dir, "%s/themes",app_usrdir);
	mkdir(themes_dir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
	sprintf(themes_web_dir, "%s/themes_web",app_usrdir);
	mkdir(themes_web_dir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
	

	parse_ini(options_ini,0);
	if(theme_sound)
	{
		sprintf(bootmusic,  "%s/SOUND.BIN", app_usrdir);
		if(stat(bootmusic, &s3)>=0) main_mp3((char*)bootmusic);
	}

//	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
//	cellDbgFontPrintf( 0.32f, 0.85f, 1.0f, 0x90909090, "multiMAN is allocating memory...");
//	cellDbgFontDrawGcm(); setRenderColor();	flip();






//reboot:
//	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
//	cellDbgFontPrintf( 0.30f, 0.85f, 1.0f, 0x90909090, "multiMAN is loading user options...");
//	cellDbgFontDrawGcm(); setRenderColor();	flip();
//	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
//	cellDbgFontPrintf( 0.31f, 0.85f, 1.0f, 0x90909090, "multiMAN is loading textures...");
//	cellDbgFontDrawGcm(); setRenderColor();	flip();

		for(usb_loop=0;usb_loop<8;usb_loop++)
		{
			sprintf(filename, "/dev_usb00%i/COLOR.INI", usb_loop);
			if(stat(filename, &s2)>=0) {
				sprintf(string1,  "%s/COLOR.INI", app_usrdir);
				file_copy(filename, string1, 0); break;
			}
		}


	sprintf(app_temp, "%s/TEMP",app_usrdir);
	del_temp(app_temp);

	rnd=time(NULL)&0x03;
	sprintf(auraBG, "%s/AUR%i.JPG",app_usrdir, rnd);
	sprintf(userBG, "%s/PICBG.JPG",app_usrdir);

	sprintf(xmbicons, "%s/XMB.PNG", app_usrdir);
	sprintf(xmbicons2, "%s/XMB2.PNG", app_usrdir);
	sprintf(xmbdevs, "%s/XMB64.PNG", app_usrdir);
	sprintf(xmbbg, "%s/XMBBG.PNG", app_usrdir);

	sprintf(filename, "%s/XMBR.PNG", app_usrdir); remove(filename);

	sprintf(filename, "%s/DOX.PNG", app_usrdir);
	load_texture(text_DOX, filename, dox_width);

	sprintf(playBGR, "%s/PICPA.PNG",app_usrdir);

/*	sprintf(blankBG, "/dev_hdd0/game/%s/PIC0.PNG",app_path);
	sprintf(filename, "%s/XMB0.PNG",app_usrdir);
	if(stat(filename, &s2)>=0 && stat(blankBG, &s2)<0 ) file_copy(filename, blankBG, 0);

	sprintf(blankBG, "/dev_hdd0/game/%s/PIC1.PNG",app_path);
	sprintf(filename, "%s/XMB1.PNG",app_usrdir);
	if(stat(filename, &s2)>=0 && stat(blankBG, &s2)<0 ) file_copy(filename, blankBG, 0); */

	sprintf(blankBG, "/dev_hdd0/game/%s/ICON0.PNG",app_path);
	sprintf(filename, "%s/ICON0.PNG",app_usrdir);
	if(stat(filename, &s2)>=0) file_copy(filename, blankBG, 0);

	if(V_WIDTH>1280)
		sprintf(filename, "%s/MP_HR.PNG",app_usrdir);
	else
	{
		sprintf(filename, "%s/MP_LR.PNG",app_usrdir);
		mp_WIDTH=15, mp_HEIGHT=21; //mouse icon LR
	}

	if(stat(filename, &s2)>=0) {load_texture((unsigned char *) mouse, filename, mp_WIDTH);}// gray_texture(mouse, 32, 32);}

	sprintf(helpNAV, "%s/NAV.JPG",app_usrdir);
	sprintf(helpMME, "%s/help.MME",app_usrdir);

	sprintf(ps2png, "%s/PS2.JPG",app_usrdir);
	sprintf(dvdpng, "%s/DVD.JPG",app_usrdir);

	sprintf(avchdIN, "%s/AVCIN.DAT",app_usrdir);
	sprintf(avchdMV, "%s/AVCMV.DAT",app_usrdir);

	sprintf(iconHDD, "%s/HDD.JPG",app_usrdir);
	sprintf(iconUSB, "%s/USB.JPG",app_usrdir);
	sprintf(iconBLU, "%s/BLU.JPG",app_usrdir);
	sprintf(iconNET, "%s/NET.JPG",app_usrdir);
	sprintf(iconOFF, "%s/OFF.JPG",app_usrdir);

	sprintf(iconCFC, "%s/CFC.JPG",app_usrdir);
	sprintf(iconSDC, "%s/SDC.JPG",app_usrdir);
	sprintf(iconMSC, "%s/MSC.JPG",app_usrdir);
	sprintf(playBG,  "%s/FMS.PNG",app_usrdir);

//	if(c_firmware==3.41f)
//		sprintf(legend, "%s/LEGEND0.PNG", app_usrdir);
//	else
	sprintf(legend, "%s/LEGEND2.PNG", app_usrdir);

	sprintf(versionUP,  "%s/VERSION.DAT",app_usrdir);



	sprintf(filename,  "%s/LBOX.PNG",app_usrdir);
	if(stat(filename, &s3)<0 || stat(xmbdevs, &s3)<0) {
		dialog_ret=0;
		ret = cellMsgDialogOpen2( type_dialog_ok, "WARNING:\n\nYour installation of multiMAN is incomplete!\nPlease install BASE or FULL version or you may experience graphics display problems!", dialog_fun2, (void*)0x0000aaab, NULL );					
		wait_dialog();
	}
	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	set_texture( text_bmpUBG, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
	flip();

//	load_texture(text_bmpBG, userBG, 1920);

	load_texture(text_bmpUPSR, playBGR, 1920);

	load_texture(text_bmpIC, blankBG, 320);
	load_texture(text_HDD, iconHDD, 320);
	load_texture(text_USB, iconUSB, 320);

	load_texture(text_BLU_1, iconBLU, 320);
	load_texture(text_NET_6, iconNET, 320);
	load_texture(text_OFF_2, iconOFF, 320);
	load_texture(text_CFC_3, iconCFC, 320);
	load_texture(text_SDC_4, iconSDC, 320);
	load_texture(text_MSC_5, iconMSC, 320);

//	sprintf(mmbin, "%s/RELOAD.SELF", app_usrdir);

	sprintf(filename, "/dev_hdd0/game/%s/GAMES", app_path);
	if(stat(filename, &s3)>=0) rmdir(filename);
//	mkdir(filename, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); 

	sprintf(filename, "%s/XMB Video", app_usrdir);
	if(stat(filename, &s3)<0) mkdir(filename, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

	sprintf(cache_dir, "/dev_hdd0/game/%s/cache", app_path);
	sprintf(filename, "%s/cache", app_usrdir);
	if(stat(filename, &s3)<0) rename(cache_dir, filename);
	sprintf(cache_dir, "%s/cache", app_usrdir);
	if(stat(filename, &s3)<0)
	{
		is_reloaded=0;
		mkdir(cache_dir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
	}
	sprintf(filename, "%s/.mmcache002", cache_dir);
	if(stat(filename, &s3)<0) {is_reloaded=0; file_copy(disclaimer, filename, 0); max_menu_list=0; forcedevices=0xffff;}
	//fix_perm_recursive(cache_dir);

	sprintf(filename, "%s/TEMP",app_usrdir);
	mkdir(filename, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

	sprintf(game_cache_dir, "%s/game_cache",app_usrdir);
	mkdir(game_cache_dir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);



//	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); //draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
//	set_texture( text_bmpUBG, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//	cellDbgFontPrintf( 0.27f, 0.85f, 1.0f, 0x90909090, "multiMAN is checking system configuration...");
//	cellDbgFontDrawGcm(); setRenderColor();	flip();

	sc36_path_patch=0;

    payload = -1;  // 0->psgroove  1->hermesV3/V4  2->PL3  
	payloadT[0]=0x20;

	if(sys8_enable(0) > 0)
	{
		payload=1; payloadT[0]=0x48; //H
		ret = sys8_enable(0);
		ret = sys8_path_table(0ULL);
	}

	else
	{
		payload=0;
		if (syscall35("/dev_hdd0", "/dev_hdd0") == 0)
		{
			payload=2; payloadT[0]=0x50; //P
		}
		else
		{

			payload=0;
			if(peekq(0x8000000000346690ULL) == 0x80000000002BE570ULL && peekq(0x80000000002D8538ULL) == 0x7FA3EB784BFDAD60ULL)
			{
				pokeq(0x80000000002D8498ULL, 0x38A000064BD7623DULL ); // 09 symbols search
				pokeq(0x80000000002D8504ULL, 0x38A000024BD761D1ULL ); // 0x002D7800 (/app_home) 2 search
//				enable_sc36();
				/*
				pokeq(0x80000000002D8600ULL, 0x2F6465765F626476ULL ); // /dev_bdvd
				pokeq(0x80000000002D8608ULL, 0x6400000000000000ULL );

				pokeq(0x80000000002D8700ULL, 0x2F6170705F686F6DULL ); // /app_home
				pokeq(0x80000000002D8708ULL, 0x6500000000000000ULL );

				pokeq(0x80000000002D84D5ULL, 0x0000000000000000ULL ); // remove old dev_bdvd string
				pokeq(0x80000000002D84DFULL, 0x0000000000000000ULL ); // remove old app_home string

				pokeq(0x80000000002D8490ULL, 0x67E4002D60848600ULL ); // 0x002D8600 (/dev_bdvd)
				pokeq(0x80000000002D8498ULL, 0x38A000094BD7623DULL ); // 09 symbols search
				pokeq(0x80000000002D84C0ULL, 0x389D00094BD761C1ULL ); // 09 symbols replace

				pokeq(0x80000000002D8500ULL, 0x6084870038A00009ULL ); // 0x002D7800 (/app_home) 9 search
				pokeq(0x80000000002D8528ULL, 0xE87F0008389D0009ULL ); // 9 replace 
				*/

			}//D

			payload=-1;
			system_call_1(36, (uint32_t) app_usrdir);
			sprintf(filename, "%s", "/dev_bdvd/EBOOT.BIN");
			if(stat(filename, &s2)>=0)
			{
				payload=0; payloadT[0]=0x47; //G
				if(peekq(0x8000000000346690ULL) == 0x80000000002BE570ULL && peekq(0x80000000002D8538ULL) == 0x7FA3EB784BFDAD60ULL) {payloadT[0]=0x44; sc36_path_patch=1; }//D
			}

		}
	}   

	load_custom_payload=0; //force PG

	sprintf(filename, "%s/BDEMU.BIN", app_usrdir);
	if(stat(filename, &s2)>=0)
	{
		fpV = fopen ( filename, "rb" );
		fseek(fpV, 0, SEEK_END);
		u32 len=ftell(fpV);
		fclose(fpV);
		if(len==488) {bd_emulator=2;bdemu2_present=0;}
		if(len==4992) {bdemu2_present=1;}
		
	}
	if(c_firmware == 3.55f && payload==-1 && load_custom_payload != 1 && stat(filename,&s2)>=0 && bd_emulator==2) 
	{
		fpV = fopen ( filename, "rb" );
		fseek(fpV, 0, SEEK_SET);
		fread((void *) bdemu, 488, 1, fpV);
		fclose(fpV);

//		sprintf(filename, "%s/BDEMU355.BIN", app_usrdir);
//		fpV = fopen ( filename, "rb" );
//		fseek(fpV, 0, SEEK_SET);
//		fread((void *) bdemu355, 576, 1, fpV);
//		fclose(fpV);

//		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); //draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
//		set_texture( text_bmpUBG, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//		cellDbgFontPrintf( 0.27f, 0.85f, 1.0f, 0x90909090, "multiMAN is enabling BD-ROM emulator...");
//		cellDbgFontDrawGcm(); setRenderColor();	flip();
		payload=0;
		psgroove_main(0);

		if(peekq(0x8000000000346690ULL) == 0x80000000002BE570ULL){
			payload=0; 
			if(peekq(0x80000000002D8538ULL) == 0x7FA3EB784BFDAD60ULL )
			{
				payloadT[0]=0x44; //D
				sc36_path_patch=1;
			}
			else
			{
				payloadT[0]=0x47; //G
				sc36_path_patch=0;
			}
		}
		else
		{
			payload=-1; payloadT[0]=0x20; //NONE
		}
	}


	sprintf(filename, "%s/BDEMU.BIN", app_usrdir);
	if(c_firmware == 3.55f && payload==-1 && load_custom_payload != 1 && stat(filename,&s2)>=0 && bd_emulator==1) 
	{
		fpV = fopen ( filename, "rb" );

		fseek(fpV, 0, SEEK_END);
		u32 len=ftell(fpV);
		if(len>=4992)
		{
			fseek(fpV, 488, SEEK_SET);
			fread((void *) bdemu, 1024, 1, fpV);
			fclose(fpV);
			payload=0;
			hermes_payload_355(0);

			if(sys8_enable(0) > 0)
			{
				payload=1; payloadT[0]=0x48; //H
				ret = sys8_enable(0);
				ret = sys8_path_table(0ULL);
				//ret = sys8_perm_mode(1);
				bdemu2_present=1;
			}
			else
			{
				payload=-1; payloadT[0]=0x20; //NONE
			}
		}
		else
		{
			payload=-1; payloadT[0]=0x20; //NONE
			fclose(fpV);
			dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, "multiMAN cannot enable selected BD-ROM emulator type. Functionality may be restricted!\n\nError: BDEMU.BIN incorrect version", dialog_fun2, (void*)0x0000aaab, NULL );wait_dialog();
		}
	}

	sprintf(filename, "%s/BDEMU.BIN", app_usrdir);
	if(c_firmware == 3.41f && payload==-1 && load_custom_payload != 1 && stat(filename,&s2)>=0 && bd_emulator==1)
	{
		fpV = fopen ( filename, "rb" );
		fseek(fpV, 0, SEEK_END);
		u32 len=ftell(fpV);
		if(len>=4992)
		{
			fseek(fpV, 1512, SEEK_SET);
			fread((void *) bdemu, 3480, 1, fpV);
			fclose(fpV);
			payload=0;
			hermes_payload_341();
			sys_timer_usleep(250000);

			if(sys8_enable(0) > 0)
			{
				payload=1; payloadT[0]=0x48; //H
				ret = sys8_enable(0);
				ret = sys8_path_table(0ULL);
				bdemu2_present=1;
			}
			else
			{
				payload=-1; payloadT[0]=0x20; //NONE
			}
		}
		else
		{
			payload=-1; payloadT[0]=0x20; //NONE
			fclose(fpV);
			dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, "multiMAN cannot enable selected BD-ROM emulator type. Functionality may be restricted!\n\nError: BDEMU.BIN incorrect version", dialog_fun2, (void*)0x0000aaab, NULL );wait_dialog();
		}
	}

//	cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A); //draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
//	set_texture( text_bmpUBG, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//	cellDbgFontPrintf( 0.29f, 0.85f, 1.0f, 0x90909090, "multiMAN initialization completed.");
//	cellDbgFontDrawGcm(); setRenderColor();	
//	flip();
//	sprintf(avchdBG, "%s/AVCHD.JPG",app_usrdir);

	//parse_ini(options_ini,0);
	parse_color_ini();

//	if(load_last_state==1) 
	parse_last_state();
	clean_up();

	if(payload==-1)
	{
		if(bd_emulator)
		{
			dialog_ret=0;
			sprintf(filename, "%s/BDEMU.BIN", app_usrdir);
			if(stat(filename, &s2)<0)
				cellMsgDialogOpen2( type_dialog_ok, "multiMAN cannot enable BD-ROM emulator. Functionality may be restricted!\n\nError: BDEMU.BIN missing", dialog_fun2, (void*)0x0000aaab, NULL );
			else
				cellMsgDialogOpen2( type_dialog_ok, "multiMAN cannot enable BD-ROM emulator. Functionality may be restricted!\n\nError: Unsupported system firmware or BDEMU.BIN incorrect version", dialog_fun2, (void*)0x0000aaab, NULL );
			wait_dialog();
		}
		if(c_firmware == 3.55f) psgroove_main(1);

	}
	else
		reset_mount_points();
	disable_sc36();

//    if(payload==2) progress_bar=0; //c_firmware<3.55f && 

	if(cover_mode==3) load_texture(text_FONT, userBG, 1920);

	if(cover_mode<3 || cover_mode>5)
		draw_legend=1;
	else
		draw_legend=0;

	if(c_firmware==3.41f && peekq(0x80000000000505d0ULL) == memvalnew)
		patchmode = 1;
	else
		patchmode = 0;

	if(mount_hdd1==1)
	{
		memset(&cache_param, 0x00 , sizeof(CellSysCacheParam)) ;
		strncpy(cache_param.cacheId, app_path, sizeof(cache_param.cacheId)) ;
		cellSysCacheMount( &cache_param ) ;
//		sprintf(sys_cache, "%s", cache_param.getCachePath);
		remove( (char*) "/dev_hdd1/multiMAN" );
		remove( (char*) "/dev_hdd1/multiMAN.srt" );
		remove( (char*) "/dev_hdd1/multiMAN.ssa" );
		remove( (char*) "/dev_hdd1/multiMAN.ass" );
	}

	
	for(int n=0;n<max_hosts;n++)
		if(host_list[n].port>0)
			remove(host_list[n].name);
//			ret = network_com((char*)"GET", (char*)host_list[n].host, host_list[n].port, (char*)"/", (char*) host_list[n].name, 1);//host_list[n].root

	for(int n=0; n<MAX_STARS;n++)
	{
		stars[n].x=rndv(1920);
		stars[n].y=rndv(1080);
		stars[n].bri=rndv(128);
		stars[n].size=rndv(XMB_SPARK_SIZE)+1;
	}

	mkdir(covers_dir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
	DIR  *dir;dir=opendir (ini_hdd_dir); if(!dir) mkdir(ini_hdd_dir, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); else closedir(dir);
	strncpy(hdd_folder, ini_hdd_dir, 64);
	dir=opendir (ini_hdd_dir); 
	if(!dir){
		dialog_ret=0; force_disable_copy=1;
		ret = cellMsgDialogOpen2( type_dialog_ok, "CRITICAL ERROR:\n\nmultiMAN cannot access or create default backup folder!\nGame backup functions may not work properly.\n\nPlease use different payload if necessary!", dialog_fun2, (void*)0x0000aaab, NULL );					
		wait_dialog();
	}
	else closedir(dir);

//	mkdir(hdd_folder, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
//	fix_perm_recursive(hdd_folder);

	int find_device=0;

//system_call_8(837, (u64)device, (u64)format, (u64)point, a, b, c, (u64)buffer, len);

//  pokeq( 0x80000000003EF154ULL, 0x4641540000000000ULL); //FAT
//  pokeq( 0x80000000003EF5e4ULL, 0x4641540000000000ULL);

//  pokeq( 0x80000000003EF154ULL, 0x5544460000000000ULL); //UDF

//  pokeq( 0x80000000003EF5e4ULL, 0x5544460000000000ULL);

//  pokeq( 0x80000000003EF60DULL, 0x3241380000000000ULL);
//  pokeq( 0x80000000003EF605ULL, 0x3242E88000000000ULL);

//  pokeq( 0x800000000018F6B0ULL, 0x480000483C008001ULL ); //40 9E 00 48 3C 00 80 01
//  pokeq( 0x800000000018F720ULL, 0x480001202F9F0000ULL ); //41 9E 01 20 2F 9F 00 00
//  pokeq( 0x8000000000192480ULL, 0x4800004C4092FEC4ULL ); //40 9E 00 4C 40 92 FE C4
//pokeq( 0x8000000000192520ULL, 0x600000002FBA0000ULL ); //41 82 02 DC 2F BA 00 00  !! PROBLEM
//pokeq( 0x800000000019282CULL, 0x63BD06664BFFD67DULL ); //63 BD 00 2B 4B FF D6 7D

//  pokeq( 0x800000000019282CULL, 0x63BD00004BFFD67DULL ); //63 BD 00 2B 4B FF D6 7D

//uint64_t ret2=0x00ULL;

//	ret2 = syscall_838("/dev_usb000");
//	ret2 = syscall_838("/dev_usb001");
//	ret2 = syscall_838("/dev_bdvd");
//	ret2 = syscall_838("/app_home");

//	ret2 = syscall_837("CELL_FS_IOS:USB_MASS_STORAGE000", "CELL_FS_UDF", "/dev_bdvd", 0, 1, 0, 0, 0);
//					sprintf(filename, "Error (%08X) occured!", (u32)ret2);
//					dialog_ret=0;
//					ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );
//					wait_dialog();

//CELL_FS_IOS:BDVD_DRIVE
//	ret2 = syscall_837("CELL_FS_IOS:USB_MASS_STORAGE000", "CELL_FS_FAT", "/dev_bdvd", 0, 1, 0, 0, 0);
//	ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_UDF", "/dev_bdvd", 0, 1, 0, 0, 0);
//	ret2 = syscall_837("CELL_FS_IOS:PATA0_BDVD_DRIVE", "CELL_FS_UDF", "/dev_bdvd", 0, 1, 0, 0, 0);
//	ret2 = syscall_837("CELL_FS_IOS:PATA0_BDVD_DRIVE", "CELL_FS_UDF", "/dev_usb000", 0, 1, 0, 0, 0);
//	ret2 = syscall_837("CELL_FS_IOS:PATA1_BDVD_DRIVE", "CELL_FS_UDF", "/app_home", 0, 1, 0, 0, 0);
//	ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_ISO9660", "/dev_bdvd", 0, 1, 0, 0, 0);


/*

	ret2 = syscall_837("CELL_FS_IOS:USB_MASS_STORAGE", "CELL_FS_UDF", "/dev_usb", 0, 0, 0, 0, 0);
					sprintf(filename, "Error (%08X) occured!", (u32)ret2);
					dialog_ret=0;
					ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );
					wait_dialog();

	ret2 = syscall_838("/dev_hdd1");

					sprintf(filename, "Error (%08X) occured!", (u32)ret2);
					dialog_ret=0;
					ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );
					wait_dialog();

	ret2 = syscall_837("CELL_FS_UTILITY:HDD1", "CELL_FS_UDF", "/dev_hdd1", 0, 0, 0, 0, 0);
					sprintf(filename, "Error (%08X) occured!", (u32)ret2);
					dialog_ret=0;
					ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );
					wait_dialog();


//int ret = lv2FsMount("CELL_FS_IOS:USB_MASS_STORAGE006", "CELL_FS_UDF", "/dev_usb006", 0, 0, 0, 0, 0);

*/


	game_last_page=-1;
	last_selected=-1;
	use_depth=(cover_mode!=8);
    uint64_t nread;
    int dir_fd;
    CellFsDirent entryF;

	net_avail=cellNetCtlGetInfo(16, &net_info);
	init_finished=1;


	sys_ppu_thread_create( &download_thr_id, download_thread_entry,
						   NULL, 2048, app_stack_size,
						   0, "multiMAN_downqueue" );

	sys_ppu_thread_create( &misc_thr_id, misc_thread_entry,
						   NULL, 512, app_stack_size,
						   0, "multiMAN_misc" );//SYS_PPU_THREAD_CREATE_JOINABLE


	ss_timer=0;
	ss_timer_last=time(NULL);

	get_free_memory();

	if (meminfo.avail<16777216)
	{
			dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, "Please restart multiMAN from PS3 XMB\xE2\x84\xA2", dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
			unload_modules(); sys_process_exit(1);
	}


	/* main loop */
	while( pad_read() != 0)
	{
start_of_loop:

	/*if(to_reboot){
		to_reboot=0;
		unload_modules();
		cellSysmoduleLoadModule( CELL_SYSMODULE_GCM_SYS );
		ret = load_modules();
		if(ret!=CELL_OK && ret!=0) sys_process_exit(1);

		MEMORY_CONTAINER_SIZE_ACTIVE = MEMORY_CONTAINER_SIZE;
		ret = sys_memory_container_create( &memory_container, MEMORY_CONTAINER_SIZE_ACTIVE);
		if(ret!=CELL_OK) 
		{
			MEMORY_CONTAINER_SIZE_ACTIVE = MEMORY_CONTAINER_SIZE - (2 * 1024 * 1024);
			ret = sys_memory_container_create( &memory_container, MEMORY_CONTAINER_SIZE_ACTIVE);
		}
		if(ret!=CELL_OK) sys_process_exit(1);
		first_launch=1;

		goto reboot;
	}*/

	if(dim_setting>0)
		{
			dimc++; 
			if( (dimc>(dim_setting*80) && cover_mode!=5) || ( (cover_mode==5 || cover_mode==8) && dimc>(dim_setting*80*7)) ) {dimc=0; dim=1;c_opacity_delta=-1;}
		}
	else
		{	dimc=0; dim=0; c_opacity_delta=0;}
	// scan for plug/unplug devices
	
	int count_devices=0;
	int dev_changed=0;
	int dev_removed=0;
	int to_restore=1;

	if(one_time) {one_time=0;goto skip_find_device;}
	CellFsStat status;

//	for(find_device=0;find_device<16;find_device++)
//    if (cellFsOpendir((char*) "/", &dir_fd) == CELL_FS_SUCCEEDED)

	if( (time(NULL)-last_refresh)>1 || first_launch || forcedevices!=0 )
	{
		u32 fdevices0=0;
		cellFsOpendir((char*) "/", &dir_fd);
		while(1) {
			last_refresh=time(NULL);
			cellFsReaddir(dir_fd, &entryF, &nread);
			if(nread==0) break;
			
			sprintf(filename, "%s", entryF.d_name);
			if(strstr(filename,"app_home")!=NULL || strstr(filename,"dev_hdd1")!=NULL) continue;
			find_device=-1;

			if(strstr(filename,"dev_hdd0")!=NULL) find_device=0;
			else if(strstr(filename,"dev_usb00")!=NULL) find_device=(filename[9])-0x2f;
			else if(strstr(filename,"dev_bdvd")!=NULL && hide_bd==0) find_device=11;
			else if(strstr(filename,"host_root")!=NULL && pfs_enabled) find_device=13;
			else if(strstr(filename,"dev_sd")!=NULL) find_device=14;
			else if(strstr(filename,"dev_ms")!=NULL) find_device=15;

			if(find_device!=-1) fdevices0|=(1<<find_device);
		}
		cellFsClosedir(dir_fd);

#if (CELL_SDK_VERSION>0x210001)
			if ( ( (fdevices0>>13) & 1 ) && pfs_enabled) 
			{
				sprintf(filename, "/pvd_usb000");
				if (PfsmVolStat(0) == 0) 
					fdevices0 |= 1 << 13;
				else 
					fdevices0 &= ~(1 << 13);
			}
			else 
#endif
				fdevices0 &= ~(1 << 13);

		for(find_device=0;find_device<16;find_device++)
		{

			//if(find_device==13) sprintf(filename, "/dev_none"); else sprintf(filename, "/%s", entryF.d_name);

/*			if(find_device>0 && find_device<10) sprintf(filename, "/dev_usb00%i", find_device-1);
			if(find_device==11) sprintf(filename, "/dev_bdvd");
			if(find_device==0) sprintf(filename, "/dev_hdd0");
			if(find_device==13) sprintf(filename, "/dev_none");
			if(find_device==14) sprintf(filename, "/dev_sd");
			if(find_device==15) sprintf(filename, "/dev_ms"); */

			{
//				if((cellFsStat(filename, &status) == CELL_FS_SUCCEEDED && ((forcedevices>>find_device) & 1)==0) || find_device==0)
				if( (( (fdevices0>>find_device) & 1 ) ))// && ((forcedevices>>find_device) & 1)==0))// || find_device==0
				{
					fdevices|= 1<<find_device;
				}
				else
					fdevices&= ~ (1<<find_device);
			}

			// limit to 16 devices
			if(((fdevices>>find_device) & 1) && find_device!=11)
			{
				count_devices++;
				if(count_devices>14) fdevices&= ~ (1<<find_device);
			}

			// bdvd
			if(find_device==11)
			{

				if(((forcedevices>>find_device) & 1) || ((fdevices>>find_device) & 1)!=((fdevices_old>>find_device) & 1)) //fdevices!=fdevices_old || 
				{
					c_opacity_delta=16;	dimc=0; dim=1;
					dev_changed=1;
						
					if( ((fdevices>>11) & 1) && hide_bd==0)
					{

						sprintf(filename, "/dev_bdvd/PS3_GAME/PARAM.SFO");
						bluray_game[0]=0;
						parse_param_sfo(filename, bluray_game, bluray_id, &bluray_pl); bluray_game[63]=0;
						sprintf(filename, "%s/%s_240.RAW", cache_dir, bluray_id);
						if(stat(filename, &s3)<0) 
						{
							sprintf(filename, "%s", "/dev_bdvd/PS3_GAME/PIC1.PNG");
							cache_png(filename, bluray_id);
						}

						if(max_menu_list>=MAX_LIST) max_menu_list= MAX_LIST-1;
						
						sprintf(menu_list[max_menu_list].path, "/dev_bdvd");
						memcpy(menu_list[max_menu_list].title, bluray_game, 63);
						sprintf(menu_list[max_menu_list].title_id, "%s", bluray_id);
						menu_list[max_menu_list].title[63]=0;
						menu_list[max_menu_list].flags=(1<<11);
						sprintf(menu_list[max_menu_list ].content, "%s", "PS3");
						menu_list[max_menu_list].plevel=bluray_pl;
						menu_list[max_menu_list].user=0;
						menu_list[max_menu_list].split=0;
						if(stat((char*) "/dev_bdvd/PS3_GAME/PARAM.SFO", &s3)<0) 
						{
							menu_list[max_menu_list ].flags=(1<<11);
							sprintf(filename, "[Disc] Data Disc");
							strncpy(menu_list[max_menu_list ].title, filename, 63);
							menu_list[max_menu_list ].title[63]=0;
							menu_list[max_menu_list ].cover=-1;
							sprintf(menu_list[max_menu_list ].path, "/dev_bdvd");
							sprintf(menu_list[max_menu_list ].entry, "Data Disc");
							sprintf(menu_list[max_menu_list ].content, "%s", "DATA");
							sprintf(menu_list[max_menu_list ].title_id, "%s", "NO_ID");

						}

//check for PS2/DVD

						struct stat s;

						sprintf(filename, "/dev_bdvd/BDMV/index.bdmv");

						if(stat(filename, &s)>=0) { 

							menu_list[max_menu_list ].flags=(1<<11);
							sprintf(filename, "[BDMV] Blu-ray\xE2\x84\xA2 Video Disc");
							strncpy(menu_list[max_menu_list ].title, filename, 63);
							menu_list[max_menu_list ].title[63]=0;
							menu_list[max_menu_list ].cover=-1;
							sprintf(menu_list[max_menu_list ].path, "/dev_bdvd");
							sprintf(menu_list[max_menu_list ].entry, "[BDMV]");
							sprintf(menu_list[max_menu_list ].content, "%s", "BDMV");
							sprintf(menu_list[max_menu_list ].title_id, "%s", "NO_ID");
						}


						sprintf(filename, "/dev_bdvd/SYSTEM.CNF");
						if(stat(filename, &s)<0) sprintf(filename, "/dev_bdvd/system.cnf");

						if(stat(filename, &s)>=0) { 

							menu_list[max_menu_list ].flags=(1<<11);
							sprintf(filename, "[PS2] Disc");
							strncpy(menu_list[max_menu_list ].title, filename, 63);
							menu_list[max_menu_list ].title[63]=0;
							menu_list[max_menu_list ].cover=-1;
							sprintf(menu_list[max_menu_list ].path, "/dev_bdvd");
							sprintf(menu_list[max_menu_list ].entry, "PS2 Game");
							sprintf(menu_list[max_menu_list ].content, "%s", "PS2");
							sprintf(menu_list[max_menu_list ].title_id, "%s", "NO_ID");
						}
						else
						{
							sprintf(filename, "/dev_bdvd/VIDEO_TS/VIDEO_TS.IFO");
							if(stat(filename, &s)>=0) { 
				
								menu_list[max_menu_list ].flags=(1<<11);
								sprintf(filename, "[DVD Video] Video Disc");
								strncpy(menu_list[max_menu_list ].title, filename, 63);
								menu_list[max_menu_list ].title[63]=0;
								menu_list[max_menu_list ].cover=-1;
								sprintf(menu_list[max_menu_list ].path, "/dev_bdvd/VIDEO_TS");
								sprintf(menu_list[max_menu_list ].entry, "DVD Video Disc");
								sprintf(menu_list[max_menu_list ].content, "%s", "DVD");
								sprintf(menu_list[max_menu_list ].title_id, "%s", "NO_ID");
							}
/*							else
							{
								menu_list[max_menu_list ].flags=(1<<11);
								sprintf(filename, "[Disc] Data");
								strncpy(menu_list[max_menu_list ].title, filename, 63);
								menu_list[max_menu_list ].title[63]=0;
								menu_list[max_menu_list ].cover=-1;
								sprintf(menu_list[max_menu_list ].path, "/dev_bdvd");
								sprintf(menu_list[max_menu_list ].entry, "Data Disc");
								sprintf(menu_list[max_menu_list ].content, "%s", "Data");
								sprintf(menu_list[max_menu_list ].title_id, "%s", "NO_ID");
							}*/

						}

						max_menu_list++;

					}
					else
					{	delete_entries(menu_list, &max_menu_list, (1<<11)); dev_removed=1; }

					sort_entries(menu_list, &max_menu_list );
					old_fi=-1;
					game_last_page=-1;

					forcedevices &= ~ (1<<find_device);
					fdevices_old&= ~ (1<<find_device);
					fdevices_old|= fdevices & (1<<find_device);
					if(game_sel>max_menu_list-1) game_sel=max_menu_list-1;
				}
				
			} //end of bdrom disc
			else
			{ //other devices
				if(((forcedevices>>find_device) & 1) || ((fdevices>>find_device) & 1)!=((fdevices_old>>find_device) & 1)) //fdevices!=fdevices_old || 
				{
					if(to_restore) {if(cover_mode!=8) set_fm_stripes(); to_restore=0;}
					dev_changed=1;
//					game_sel=0;
					old_fi=-1; game_last_page=-1;
					state_read=1; first_left=0; first_right=0;
					state_draw=1;
					if(find_device==0) 
						sprintf(filename, "%s", hdd_home);
					else 
					{
						if(strstr (usb_home,"/")!=NULL)
	  						sprintf(filename, "/dev_usb00%c%s", 47+find_device, usb_home);
						else
	  						sprintf(filename, "/dev_usb00%c/%s", 47+find_device, usb_home);

						if(find_device==13 && !pfs_enabled) sprintf(filename, "/dev_none");
						if(find_device==13 && pfs_enabled) sprintf(filename, "/pvd_usb000%s", usb_home);

						if(find_device==14) sprintf(filename, "/dev_sd%s", usb_home);
						if(find_device==15) sprintf(filename, "/dev_ms%s", usb_home);

					}


					if((fdevices>>find_device) & 1) 
						{
						
						if(strstr (filename,"/dev_hdd")!=NULL){
							if(is_reloaded && (strstr(hdd_home_2,"/dev_usb")!=NULL || strstr(hdd_home_3,"/dev_usb")!=NULL || strstr(hdd_home_4,"/dev_usb")!=NULL || strstr(hdd_home_5,"/dev_usb")!=NULL) )
								{max_menu_list=0; is_reloaded=0;}
							fill_entries_from_device(filename, menu_list, &max_menu_list, (1<<find_device), 0);
							if(strstr (hdd_home_2,"/dev_")!=NULL) fill_entries_from_device(hdd_home_2, menu_list, &max_menu_list, (1<<find_device), 2);
							if(strstr (hdd_home_3,"/dev_")!=NULL) fill_entries_from_device(hdd_home_3, menu_list, &max_menu_list, (1<<find_device), 2);
							if(strstr (hdd_home_4,"/dev_")!=NULL) fill_entries_from_device(hdd_home_4, menu_list, &max_menu_list, (1<<find_device), 2);
							if(strstr (hdd_home_5,"/dev_")!=NULL) fill_entries_from_device(hdd_home_5, menu_list, &max_menu_list, (1<<find_device), 2);
							if(scan_for_apps==1) fill_entries_from_device((char*)"/dev_hdd0/game", menu_list, &max_menu_list, (1<<find_device), 2);
						}

						if(strstr (filename,"/dev_usb")!=NULL){
							fill_entries_from_device(filename, menu_list, &max_menu_list, (1<<find_device), 0);
							sprintf(filename, "/dev_usb00%c%s", 47+find_device, usb_home_2);
							if(strstr (usb_home_2,"/")!=NULL && find_device!=0) fill_entries_from_device(filename, menu_list, &max_menu_list, (1<<find_device), 2);
							sprintf(filename, "/dev_usb00%c%s", 47+find_device, usb_home_3);
							if(strstr (usb_home_3,"/")!=NULL && find_device!=0) fill_entries_from_device(filename, menu_list, &max_menu_list, (1<<find_device), 2);
							sprintf(filename, "/dev_usb00%c%s", 47+find_device, usb_home_4);
							if(strstr (usb_home_4,"/")!=NULL && find_device!=0) fill_entries_from_device(filename, menu_list, &max_menu_list, (1<<find_device), 2);
							sprintf(filename, "/dev_usb00%c%s", 47+find_device, usb_home_5);
							if(strstr (usb_home_5,"/")!=NULL && find_device!=0) fill_entries_from_device(filename, menu_list, &max_menu_list, (1<<find_device), 2);
						}

						if(strstr (filename,"/dev_sd")!=NULL && stat("/dev_sd", &s3)>=0) {
							fill_entries_from_device(filename, menu_list, &max_menu_list, (1<<find_device), 0);
						}

						if(strstr (filename,"/dev_ms")!=NULL && stat("/dev_ms", &s3)>=0){
							fill_entries_from_device(filename, menu_list, &max_menu_list, (1<<find_device), 0);
						}

#if (CELL_SDK_VERSION>0x210001)
						if(strstr (filename,"/pvd_usb")!=NULL && pfs_enabled){
							int fsVol=0;
							for(fsVol=0;fsVol<(max_usb_volumes);fsVol++)
							{
								if (PfsmVolStat(fsVol) == 0) 
								{
									if(strstr (usb_home,"/")!=NULL)
										sprintf(filename, "/pvd_usb00%i%s", fsVol, usb_home);
									else
										sprintf(filename, "/pvd_usb00%i/%s", fsVol, usb_home);
									if(fsVol==0)
										fill_entries_from_device_pfs(filename, menu_list, &max_menu_list, (1<<find_device), 0);
									else
										fill_entries_from_device_pfs(filename, menu_list, &max_menu_list, (1<<find_device), 2);
								}
							}
						}
#endif

						}
					else
					{	delete_entries(menu_list, &max_menu_list, (1<<find_device)); dev_removed=1; }

					if(cover_mode<3 || cover_mode>5) draw_legend=1;

					forcedevices &= ~ (1<<find_device);
					fdevices_old&= ~ (1<<find_device);
					fdevices_old|= fdevices & (1<<find_device);
					if(game_sel>max_menu_list-1) game_sel=max_menu_list-1;
				}
			}
		}
	}
	is_game_loading=0;
	if(dev_changed){
		if(!first_launch)
		{
			xmb_icon_last=xmb_icon;
			xmb_icon_last_first=xmb[xmb_icon].first;
		}
		sort_entries(menu_list, &max_menu_list );
		if(cover_mode!=5 && cover_mode!=8 && !first_launch) load_texture(text_FMS, legend, 1665);
		if(cover_mode==8 && !first_launch) 
		{
			if(dev_removed)
			{
				reset_xmb_checked();
				xmb[6].init=0; xmb[7].init=0;
				init_xmb_icons(menu_list, max_menu_list, game_sel );
			}
			else
			{
				reset_xmb(1);
				init_xmb_icons(menu_list, max_menu_list, game_sel );
			}
		}
		dev_changed=0;
		dev_removed=0;
		c_opacity_delta=16;	dimc=0; dim=1;
		b_box_opaq= 0xfe;
		b_box_step= -4;
	}

	if(first_launch) 
	{
		sort_entries(menu_list, &max_menu_list );
		sprintf(avchdBG, "%s/AVCHD.JPG",app_usrdir);
		load_texture(text_bmpUBG, avchdBG, 1920);
		//if(cover_mode==5) load_texture(text_bmpUPSR, playBGR, 1920);
		if(cover_mode==3) load_texture(text_FONT, userBG, 1920);
		first_launch=0;
		if(cover_mode!=5) load_texture(text_FMS, legend, 1665); else set_fm_stripes();

		parse_last_state();

		if(cover_mode==8) {
			init_xmb_icons(menu_list, max_menu_list, game_sel );
//			main_video();
			if(xmb_icon_last!=6) 
			{ 
				xmb_icon=xmb_icon_last; draw_xmb_icon_text(xmb_icon);
				if(xmb_icon==5 && xmb[5].init==0) add_video_column();
				else if(xmb_icon==4 && xmb[4].init==0) add_music_column();
				else if(xmb_icon==3 && xmb[3].init==0) add_photo_column();
				else if(xmb_icon==8 && xmb[8].init==0) add_emulator_column();
				else if(xmb_icon_last_first<xmb[xmb_icon].size) { xmb[xmb_icon].first=xmb_icon_last_first; xmb_icon_last_first=0;}
			}
			else
			if(xmb_icon_last_first<xmb[xmb_icon].size) { xmb[xmb_icon].first=xmb_icon_last_first; xmb_icon_last_first=0; xmb_icon_last=0; }
		}
	}

	
//	if(no_video) goto skip_1;
//	if(cover_mode!=1 && cover_mode!=6 && cover_mode!=7) game_last_page=-1;
	if(game_sel<0) game_sel=0;

	if(is_reloaded) {is_reloaded=0; //max_menu_list=reloaded_menu_list; 
		//reload_fdevices=reload_fdevices&0xefff; //ignore bd disc 0000 0000 0000
		//sprintf(string1, "fdevices=%8X fdevices_old=%8X reload_fdevices=%8X", fdevices, fdevices_old, reload_fdevices);		dialog_ret=0;		ret = cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL );		wait_dialog();
		fdevices_old=reload_fdevices;
		reload_fdevices=0;
	}
force_reload:
	is_game_loading=0;
	use_depth=(cover_mode!=8);
	if( (old_fi!=game_sel && game_sel>=0 && game_sel<max_menu_list && max_menu_list>0 && counter_png==0 && is_sliding==0) || slide_step>19 || (cover_mode==1 && game_last_page!=int(game_sel/8)) || (cover_mode==7 && game_last_page!=int(game_sel/32)) )
		{	is_sliding=0;slideX=0; slideX1=0; slideX2=0; slideX3=0; is_sliding=0; slide_step=0;
			old_fi=game_sel;
			counter_png=10;
			if( (cover_mode!=1 && cover_mode!=7) || (cover_mode==1 && game_last_page!=int(game_sel/8)) || ( cover_mode==7 && game_last_page!=int(game_sel/32)) ) draw_legend=1;
//			if( ( (cover_mode!=1) || (cover_mode==1 && game_last_page!=int(game_sel/8))) || ( (cover_mode!=7) || (cover_mode==7 && game_last_page!=int(game_sel/32))) ) draw_legend=1;

			if(mode_list==0) 
			{
				if(cover_mode==0 )
					{
					if(strstr(menu_list[game_sel].path,"/pvd_usb")!=NULL && strstr(menu_list[game_sel].title_id, "NO_ID")==NULL)
						sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_sel].title_id);
					else
						{
							sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_sel].path);
							if(stat(filename, &s3)<0) sprintf(filename, "%s/ICON0.PNG", menu_list[game_sel].path);
						}

					if(strstr(menu_list[game_sel].content,"PS2")!=NULL)	sprintf(filename, "%s", ps2png);
					if(strstr(menu_list[game_sel].content,"DVD")!=NULL)	sprintf(filename, "%s", dvdpng);

					if(stat(filename, &s3)<0) sprintf(filename, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_sel].path);
					if(stat(filename, &s3)<0) sprintf(filename, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_sel].path);

					if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
					offX=0; offY=0;

					draw_list_text( text_bmp, 1920, 1080, menu_list, max_menu_list, game_sel | (0x10000 * ((menu_list[0].flags & 2048)!=0)), dir_mode, display_mode, cover_mode, c_opacity, 1);
					sprintf(string1, "%s/%s_320.RAW", cache_dir, menu_list[game_sel].title_id);
					if(stat(string1, &s3)>=0)
						{
							load_texture(text_bmpS, string1, 320);
							put_texture( text_bmp, text_bmpS, 320, 176, 320, 1440, 648, 2, 0xc0c0c080);
						}

					cover_available=0;
					if(strstr(menu_list[game_sel].content,"PS3")!=NULL)
					{
						sprintf(string1, "%s/%s.JPG", covers_dir, menu_list[game_sel].title_id);
						if(stat(string1, &s3)>=0) 
						{
							cover_available=1;
							goto fixed_cover_dm0;
						}
						else
							sprintf(string1, "%s/%s.PNG", covers_dir, menu_list[game_sel].title_id);

						if(stat(string1, &s3)>=0) 
						{
							cover_available=1;
							goto fixed_cover_dm0;
						}
						else
						{
							if((menu_list[game_sel].cover!=-1 && menu_list[game_sel].cover!=1))
							{
								sprintf(string1, "%s/%s.JPG", covers_dir, menu_list[game_sel].title_id);
								download_cover(menu_list[game_sel].title_id, string1);
							}
						}
						
						if(stat(string1, &s3)>=0) 
						{
							cover_available=1;
						}
						else
						{
							sprintf(string1, "%s/NOID.JPG", app_usrdir);
							cover_available=1;
						}

fixed_cover_dm0:
						if(cover_available)
						{
							load_texture(text_bmpS, string1, 320);
							put_texture( text_bmp, text_bmpS, 260, 300, 320, 1470, 300, 2, 0xc0c0c080);
						}
					}
					else
					{
						sprintf(string1, "%s/NOID.JPG", app_usrdir);
						load_texture(text_bmpS, string1, 320);
						put_texture( text_bmp, text_bmpS, 260, 300, 320, 1470, 300, 2, 0xc0c0c080);
					}

					load_texture(text_bmpS, filename, 320);
					if(menu_list[game_sel].title[0]=='_' || menu_list[game_sel].split) gray_texture(text_bmpS, 320, 320, 0);

					counter_png=10;
					}

				if(cover_mode==1 && int(game_sel/8)!=game_last_page)
				{
					legend_y=760;
					rnd=time(NULL)&0x03;
					sprintf(auraBG, "%s/AUR%i.JPG", app_usrdir, rnd);
					load_texture(text_bmp, auraBG, 1920);

//					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
//					set_texture( text_bmp, 1920, 1080); //PIC1.PNG
//					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//					flip();
//					gray_texture(text_bmp, 1920, 1080);
//					if(game_last_page==-1)
					memcpy(text_FONT, text_bmp + (1920*4*legend_y), (1920*4*legend_h));
					game_last_page=int(game_sel/8); last_selected=-1;

					int game_rel=0, c_x=0, c_y=0, c_game=0, game_rel2, alpha_cbox=1;
					int glo_box=0;
					game_rel2=int(game_sel/8)*8;

					sprintf(filename, "%s/CBOX4.PNG", app_usrdir);
					if(stat(filename, &s3)>=0) {alpha_cbox=2; load_texture(text_FONT+1024*1024*1, filename, 349);}
					else
					{
						sprintf(filename, "%s/CBOX2.PNG", app_usrdir);
						if(stat(filename, &s3)<0) {sprintf(filename, "%s/CBOX.PNG", app_usrdir); alpha_cbox=0;}
						load_texture(text_FONT+1024*1024*1, filename, 459);
					}

					sprintf(filename, "%s/GLC2.PNG", app_usrdir);
					if(stat(filename, &s3)>=0) 
					{
						load_texture(text_FONT+1024*1024*2, filename, 260);
						glo_box=1;
					}

					for (game_rel=game_rel2; (((game_rel-game_rel2)<8) && game_rel<max_menu_list); game_rel++)
						{


						cover_available=0;
						if(strstr(menu_list[game_rel].content,"PS3")!=NULL)
						{
							sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[game_rel].title_id);
							if(stat(filename, &s3)>=0) 
							{
								cover_available=1;
								goto fixed_cover;
							}
							else
								sprintf(filename, "%s/%s.PNG", covers_dir, menu_list[game_rel].title_id);

							if(stat(filename, &s3)>=0) 
							{
								cover_available=1;
								goto fixed_cover;
							}
							else
							{
								if((menu_list[game_rel].cover!=-1 && menu_list[game_rel].cover!=1))
								{
									sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[game_rel].title_id);
									download_cover(menu_list[game_rel].title_id, filename);
								}
							}
							
							if(stat(filename, &s3)>=0) 
							{
								cover_available=1;
								goto fixed_cover;
							}
							else
							{
								//menu_list[game_rel].cover=-1;
//								if(strstr(menu_list[game_rel].path, "/pvd_usb")!=NULL)
									sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_rel].title_id);
									//sprintf(filename, "%s", blankBG);
//								else
//									{
//										sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_rel].path);
										if(stat(filename, &s3)<0) sprintf(filename, "%s/ICON0.PNG", menu_list[game_rel].path);
//									}

								cover_available=0;
							}
							goto fixed_cover;
						}

						else //not a ps3 game
						{
							sprintf(filename, "%s/COVER.JPG", menu_list[game_rel].path);
							if(stat(filename, &s3) < 0 ) sprintf(filename, "%s/COVER.PNG", menu_list[game_rel].path);
							if(stat(filename, &s3)>=0) {
								cover_available=1;
								goto fixed_cover;
							}

							if(strstr(menu_list[game_rel].content,"PS2")!=NULL)	{sprintf(filename, "%s", ps2png); goto fixed_cover;}
							if(strstr(menu_list[game_rel].content,"DVD")!=NULL)	{sprintf(filename, "%s", dvdpng); goto fixed_cover;}

							sprintf(filename, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_rel].path);
		//					if(stat(filename, &s3)<0) sprintf(filename, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_rel].path);
							if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
fixed_cover:
								offX=0; offY=0;
								load_texture(text_bmpS, filename, 320);
						}

					c_game=game_rel-game_rel2;

					if(c_game<4) 
					{
						c_y=64;
						c_x= 150 + (433*c_game);
					
					}
					else 
					{
						c_y=430;
						c_x= 150 + (433*(c_game-4));
					}

//mip_texture( text_FONT, text_bmpS, 320, 300, 2); //scale to 160x150
//memcpy(text_bmpS, text_FONT, 160*150*4);
//blur_texture(text_bmpS, 320, 180, 0, 0, 320, 180, 0, 0, 1);					

					if(menu_list[game_rel].title[0]=='_' || menu_list[game_rel].split) gray_texture(text_bmpS, 320, 320, 0);
					if(cover_available==0)
						put_texture( text_bmp, text_bmpS, 320, 176, 320, c_x, c_y+124, 3, 0x0080ff80);
					else
						{
							menu_list[game_rel].cover=1;
							if(alpha_cbox!=0)
							{
								if(alpha_cbox==1)
									put_texture_with_alpha( text_bmp, text_FONT+1024*1024*1+9420, 335, 351, 459, c_x+8, c_y-19, 0, 0);
								else
									put_texture_with_alpha( text_bmp, text_FONT+1024*1024,		  349, 356, 349, c_x+1, c_y-26, 0, 0);
							}
							else
								put_texture_Galpha( text_bmp, 1920, 1080, text_FONT+1024*1024*1+9420, 335, 351, 459, c_x+8, c_y-19, 0, 0);
							put_texture( text_bmp, text_bmpS, 260, 300, 320, c_x+30, c_y, 0, 0x0080ff60);

							if(glo_box==1) put_texture_with_alpha( text_bmp, text_FONT+1024*1024*2, 260, 300, 260, c_x+30, c_y, 0, 0);
						}

//					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
//					set_texture( text_bmp, 1920, 1080); //PIC1.PNG
//					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//					flip();

					}

					counter_png=40; 
				}


				if(cover_mode==3)
					{
					if(strstr(menu_list[game_sel].title_id, "NO_ID")==NULL)
						sprintf(filename, "%s/%s_1920.PNG", cache_dir, menu_list[game_sel].title_id);
					else
						sprintf(filename, "%s/PS3_GAME/PIC1.PNG", menu_list[game_sel].path);
					if(stat(filename, &s3)<0)
					{
						sprintf(filename, "%s/PS3_GAME/PIC1.PNG", menu_list[game_sel].path);
						if(stat(filename, &s3)<0) sprintf(filename, "%s/POSTER.JPG", menu_list[game_sel].path);
						if(stat(filename, &s3)<0) sprintf(filename, "%s/POSTER.PNG", menu_list[game_sel].path);

						if(stat(filename, &s3)<0) 
							sprintf(filename, "%s", avchdBG);
						else
							if(strstr(filename, "POSTER")==NULL) //strstr (filename,"/dev_hdd0/")==NULL && 
							{
								sprintf(string1, "%s", filename);
								sprintf(filename, "%s/%s_1920.PNG", cache_dir, menu_list[game_sel].title_id);
								if(stat(filename, &s3)<0) {cache_png(string1, menu_list[game_sel].title_id); load_texture(text_FONT, userBG, 1920);}
							}
					}

					offX=-1543; offY=0;

					if(stat(filename, &s3)<0) { sprintf(filename, "%s", avchdBG); offX=0; }
					if(strstr (filename,"AVCHD.JPG")!=NULL) offX=0;

					if(game_bg_overlay==1) {

						load_texture(text_bmp, filename, 1920);
						if(menu_list[game_sel].title[0]=='_' || menu_list[game_sel].split) 
							gray_texture(text_bmp, 1920, 1080, 0);


//works
//mip_texture( text_FONT, text_bmp, 1920, 1080, -6); //scale to 320x180
//mip_texture( text_bmp, text_FONT, 320, 180, 6); //scale to 1920x1080
//blur_texture( text_bmp, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1, 2);

//mip_texture( text_FONT, text_bmp, 1920, 1080, -4); //scale to 480x270
//mip_texture( text_bmp, text_FONT, 480, 270, 4); //scale to 1920x1080
//blur_texture( text_bmp, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1, 1);

//to_565_texture( text_bmp, text_FONT, 1920, 1080);
//to_RGB_texture( text_FONT, text_bmp, 1920, 1080);


//to_333_texture( text_bmp, text_FONT, 1920, 1080);
//to_RGB3_texture( text_FONT, text_bmp, 1920, 1080);

					}

						cover_available=0;
						sprintf(filename, "%s/%s.PNG", covers_dir, menu_list[game_sel].title_id);
						if(stat(filename, &s3)<0) sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[game_sel].title_id);
						if(stat(filename, &s3)<0 && menu_list[game_sel].cover!=-1 && menu_list[game_sel].cover!=1) {download_cover(menu_list[game_sel].title_id, filename);
						//if(stat(filename, &s3)<0) menu_list[game_sel].cover=-1;
						}

						if(stat(filename, &s3)>=0) {
						cover_available=1;
						load_texture(text_bmpS, filename, 320);
						if(menu_list[game_sel].title[0]=='_' || menu_list[game_sel].split) gray_texture(text_bmpS, 320, 320, 0);
						}

					if(cover_available==0){
						sprintf(filename, "%s/COVER.JPG", menu_list[game_sel].path);
						if(stat(filename, &s3) < 0 ) sprintf(filename, "%s/COVER.PNG", menu_list[game_sel].path);
						if(stat(filename, &s3)<0) sprintf(filename, "%s/NOID.JPG", app_usrdir);

						if(stat(filename, &s3)>=0) {
						cover_available=1;
						load_texture(text_bmpS, filename, 320);
						if(menu_list[game_sel].title[0]=='_' || menu_list[game_sel].split) gray_texture(text_bmpS, 320, 320, 0);
						}
						load_texture(text_FONT, userBG, 1920);
					}

					counter_png=40; 
					}

				if(cover_mode==2)
					{
					cover_available=0;
					if(strstr(menu_list[game_sel].content,"PS2")!=NULL || strstr(menu_list[game_sel].content,"DVD")!=NULL)
					{
						sprintf(filename, "%s", avchdBG);
						goto DM2_load_textB;
					}

					if(strstr(menu_list[game_sel].content,"PS3")!=NULL)
						sprintf(filename, "%s/PS3_GAME/PIC1.PNG", menu_list[game_sel].path);
					else
						sprintf(filename, "%s/POSTER.JPG", menu_list[game_sel].path);
//					if(stat(filename, &s3)<0) sprintf(filename, "%s/POSTER.PNG", menu_list[game_sel].path);

					if(strstr(menu_list[game_sel].content,"PS3")==NULL) {
						if(stat(filename, &s3)<0 && strstr(menu_list[game_sel].path,"/pvd_usb")==NULL) 
							sprintf(filename, "%s", avchdBG);
					}
					else
						{
							sprintf(string1, "%s/PS3_GAME/PIC1.PNG", menu_list[game_sel].path);
							if(strstr(filename, "POSTER")==NULL) { //strstr (filename,"/dev_hdd0/")==NULL && 
								sprintf(filename, "%s/%s_1920.PNG", cache_dir, menu_list[game_sel].title_id);
								if(stat(filename, &s3)<0) cache_png(string1, menu_list[game_sel].title_id);
								if(stat(filename, &s3)<0) {sprintf(filename, "%s", avchdBG);}
							}
						}

DM2_load_textB:
					load_texture(text_bmp, filename, 1920);
/*
					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					set_texture( text_bmp, 1920, 1080); //PIC1.PNG
					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					flip(); */

//mip_texture( text_FONT, text_bmp, 1920, 1080, -4); //scale to 480x270
//mip_texture( text_bmp, text_FONT, 480, 270, 4); //scale to 1920x1080

//mip_texture( text_FONT, text_bmp, 1920, 1080, -3); //scale to 640x360
//mip_texture( text_bmp, text_FONT, 640, 360, 3); //scale to 1920x1080
//blur_texture( text_bmp, 1920, 1080, 0, 0, 1920, 1080, 4, 0, 1, 2);

//mip_texture( text_bmpS, text_bmp, 1920, 1080, 6); //scale to 320x180
//blur_texture(text_bmpS, 320, 180, 0, 0, 320, 180, 0, 0, 1);

					draw_list_text( text_bmp, 1920, 1080, menu_list, max_menu_list, game_sel | (0x10000 * ((menu_list[0].flags & 2048)!=0)), dir_mode, display_mode, cover_mode, c_opacity, 1);

					if(strstr(menu_list[game_sel].content,"PS2")!=NULL)	{sprintf(filename, "%s", ps2png); goto DM2_load_text;}
					if(strstr(menu_list[game_sel].content,"DVD")!=NULL)	{
						cover_available=1;
						sprintf(filename, "%s/COVER.JPG", menu_list[game_sel].path);
						if(stat(filename, &s3)<0) sprintf(filename, "%s/COVER.PNG", menu_list[game_sel].path);
						if(stat(filename, &s3)<0) {sprintf(filename, "%s", dvdpng); cover_available=0; }
						goto DM2_load_text;
					}

					if(strstr(menu_list[game_sel].content,"PS3")!=NULL)
					{
						
						sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[game_sel].title_id);
						if(stat(filename, &s3)<0) sprintf(filename, "%s/%s.PNG", covers_dir, menu_list[game_sel].title_id);
						else
						{
							cover_available=1;
							goto DM2_load_text;
						}

						if(stat(filename, &s3)<0) 
						{
							if(menu_list[game_sel].cover!=-1 && menu_list[game_sel].cover!=1) 
							{
								sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[game_sel].title_id);
								download_cover(menu_list[game_sel].title_id, filename);
								if(stat(filename, &s3)>=0) 
									cover_available=1;
								else
								{
//									if(strstr(menu_list[game_sel].path, "/pvd_usb")!=NULL)
										sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_sel].title_id);
//									else
									{
//										sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_sel].path);
										if(stat(filename, &s3)<0) sprintf(filename, "%s/ICON0.PNG", menu_list[game_sel].path);
									}
								}
							}
							else
							{
//								if(strstr(menu_list[game_sel].path, "/pvd_usb")!=NULL)
									sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_sel].title_id);
//								else
								{
//									sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_sel].path);
									if(stat(filename, &s3)<0) sprintf(filename, "%s/ICON0.PNG", menu_list[game_sel].path);
								}
							}
						}
						else
							cover_available=1;

						goto DM2_load_text;
					}


					sprintf(filename, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_sel].path);
					if(stat(filename, &s3)<0) sprintf(filename, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_sel].path);

					sprintf(string1, "%s/COVER.JPG", menu_list[game_sel].path);
					if(stat(string1, &s3)<0) sprintf(string1, "%s/COVER.PNG", menu_list[game_sel].path);


					if(stat(string1, &s3)>=0) 
					{
						cover_available=1;
						sprintf(filename,"%s",string1);
					}
					else
					{
						//menu_list[game_sel].cover=-1;
						cover_available=1;
						sprintf(filename, "%s/NOID.JPG", app_usrdir);
					}


					if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);


DM2_load_text:
					load_texture(text_bmpS, filename, 320);

					if(menu_list[game_sel].title[0]=='_' || menu_list[game_sel].split) gray_texture(text_bmpS, 320, 320, 0);
					counter_png=40;
					}

				if(cover_mode==4)
					{
					int game_icon=game_sel-3;
					offX=0; offY=0; slideX=0; slide_step=0;
					int grey=(menu_list[game_sel].title[0]=='_' || menu_list[game_sel].split);
					u32 color= (menu_list[game_sel].flags && game_sel==0)? COL_PS3DISC : ((grey==0) ?  COL_PS3 : COL_SPLIT);
					if(strstr(menu_list[game_sel].content,"AVCHD")!=NULL) color=COL_AVCHD;
					if(strstr(menu_list[game_sel].content,"BDMV")!=NULL) color=COL_BDMV;
					if(strstr(menu_list[game_sel].content,"PS2")!=NULL) color=COL_PS2;
					if(strstr(menu_list[game_sel].content,"DVD")!=NULL) color=COL_DVD;
					int tmp_legend_y=legend_y;
					legend_y=0;
					memset(text_bmp, 0, 737280);
					char str[256];
					sprintf(str, "%i of %i", game_sel+1, max_menu_list);
					if(dir_mode==1)
						put_label(text_bmp, 1920, 1080, (char*)menu_list[game_sel].title, (char*)str, (char*)menu_list[game_sel].path, color);
					else
						put_label(text_bmp, 1920, 1080, (char*)menu_list[game_sel].title, (char*)str, (char*)menu_list[game_sel].title_id, color);
					legend_y=tmp_legend_y;


					if(game_icon>=0) {
//						sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_icon].path);
						sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_icon].title_id);
						if(strstr(menu_list[game_icon].content,"PS2")!=NULL)	sprintf(filename, "%s", ps2png);
						if(strstr(menu_list[game_icon].content,"DVD")!=NULL)	sprintf(filename, "%s", dvdpng);
						if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);

						cover_available_1=0;
						sprintf(string1, "%s/%s.PNG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0) sprintf(string1, "%s/%s.JPG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0 && menu_list[game_icon].cover!=-1 && menu_list[game_icon].cover!=1) {download_cover(menu_list[game_icon].title_id, string1);
						//if(stat(string1, &s3)<0) menu_list[game_icon].cover=-1;
						}
						if(stat(string1, &s3)>=0) {
							cover_available_1=1;
							sprintf(filename,"%s",string1);
						}
						if(cover_available_1==0){
							sprintf(string1, "%s/COVER.JPG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/COVER.PNG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)>=0) {
							cover_available_1=1;
							sprintf(filename,"%s",string1);
							}
						}

						load_texture(text_BLU_1, filename, 320);
						if(menu_list[game_icon].title[0]=='_' || menu_list[game_icon].split) gray_texture(text_BLU_1, 320, 320, 0);
					}

					game_icon=game_sel-2;
					if(game_icon>=0) {
//						sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_icon].path);
						sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_icon].title_id);
						if(strstr(menu_list[game_icon].content,"PS2")!=NULL)	sprintf(filename, "%s", ps2png);
						if(strstr(menu_list[game_icon].content,"DVD")!=NULL)	sprintf(filename, "%s", dvdpng);
						if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
						cover_available_2=0;
						sprintf(string1, "%s/%s.PNG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0) sprintf(string1, "%s/%s.JPG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0 && menu_list[game_icon].cover!=-1 && menu_list[game_icon].cover!=1) {download_cover(menu_list[game_icon].title_id, string1);
						//if(stat(string1, &s3)<0) menu_list[game_icon].cover=-1;
						}
						if(stat(string1, &s3)>=0) {
							cover_available_2=1;
							sprintf(filename,"%s",string1);
						}
						if(cover_available_2==0){
							sprintf(string1, "%s/COVER.JPG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/COVER.PNG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)>=0) {
							cover_available_2=1;
							sprintf(filename,"%s",string1);
							}
						}

//						if(game_sel_last==game_sel-1) 
//							memcpy(text_OFF_2, text_CFC_3, 0x64000);
//						else
							load_texture(text_OFF_2, filename, 320);

						if(menu_list[game_icon].title[0]=='_' || menu_list[game_icon].split) gray_texture(text_OFF_2, 320, 320, 0);
					}

					game_icon=game_sel-1;
					if(game_icon>=0) {
//						sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_icon].path);
						sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_icon].title_id);
						if(strstr(menu_list[game_icon].content,"PS2")!=NULL)	sprintf(filename, "%s", ps2png);
						if(strstr(menu_list[game_icon].content,"DVD")!=NULL)	sprintf(filename, "%s", dvdpng);
						if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
						cover_available_3=0;
						sprintf(string1, "%s/%s.PNG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0) sprintf(string1, "%s/%s.JPG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0 && menu_list[game_icon].cover!=-1 && menu_list[game_icon].cover!=1) {download_cover(menu_list[game_icon].title_id, string1);
						//if(stat(string1, &s3)<0) menu_list[game_icon].cover=-1;
						}
						if(stat(string1, &s3)>=0) {
							cover_available_3=1;
							sprintf(filename,"%s",string1);
						}
						if(cover_available_3==0){
							sprintf(string1, "%s/COVER.JPG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/COVER.PNG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)>=0) {
							cover_available_3=1;
							sprintf(filename,"%s",string1);
							}
						}

//						if(game_sel_last==game_sel-1) 
//							memcpy(text_CFC_3, text_bmpS, 0x64000);
//						else
							load_texture(text_CFC_3, filename, 320);
							if(menu_list[game_icon].title[0]=='_' || menu_list[game_icon].split) gray_texture(text_CFC_3, 320, 320, 0);
					}

					game_icon=game_sel;
//					sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_icon].path);
					sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_icon].title_id);
					if(strstr(menu_list[game_icon].content,"PS2")!=NULL)	sprintf(filename, "%s", ps2png);
					if(strstr(menu_list[game_icon].content,"DVD")!=NULL)	sprintf(filename, "%s", dvdpng);
					if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
						cover_available=0;
						sprintf(string1, "%s/%s.PNG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0) sprintf(string1, "%s/%s.JPG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0 && menu_list[game_icon].cover!=-1 && menu_list[game_icon].cover!=1) {download_cover(menu_list[game_icon].title_id, string1);
						//if(stat(string1, &s3)<0) menu_list[game_icon].cover=-1;
						}
						if(stat(string1, &s3)>=0) {
							cover_available=1;
							sprintf(filename,"%s",string1);
						}
						if(cover_available==0){
							sprintf(string1, "%s/COVER.JPG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/COVER.PNG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)>=0) {
							cover_available=1;
							sprintf(filename,"%s",string1);
							}
						}

//					if(game_sel_last==game_sel-1) 
//						memcpy(text_bmpS, text_SDC_4, 0x64000);
//					else
						load_texture(text_bmpS, filename, 320);
						if(menu_list[game_icon].title[0]=='_' || menu_list[game_icon].split) gray_texture(text_bmpS, 320, 320, 0);

					game_icon=game_sel+1;
					if(game_icon<=max_menu_list) {
//						sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_icon].path);
						sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_icon].title_id);
						if(strstr(menu_list[game_icon].content,"PS2")!=NULL)	sprintf(filename, "%s", ps2png);
						if(strstr(menu_list[game_icon].content,"DVD")!=NULL)	sprintf(filename, "%s", dvdpng);
						if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
						cover_available_4=0;
						sprintf(string1, "%s/%s.PNG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0) sprintf(string1, "%s/%s.JPG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0 && menu_list[game_icon].cover!=-1 && menu_list[game_icon].cover!=1) {download_cover(menu_list[game_icon].title_id, string1);
						//if(stat(string1, &s3)<0) menu_list[game_icon].cover=-1;
						}
						if(stat(string1, &s3)>=0) {
							cover_available_4=1;
							sprintf(filename,"%s",string1);
						}
						if(cover_available_4==0){
							sprintf(string1, "%s/COVER.JPG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/COVER.PNG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)>=0) {
							cover_available_4=1;
							sprintf(filename,"%s",string1);
							}
						}

//					if(game_sel_last==game_sel-1) 
//						memcpy(text_SDC_4, text_MSC_5, 0x64000);
//					else
						load_texture(text_SDC_4, filename, 320);
						if(menu_list[game_icon].title[0]=='_' || menu_list[game_icon].split) gray_texture(text_SDC_4, 320, 320, 0);
					}

					game_icon=game_sel+2;
					if(game_icon<=max_menu_list) {
//						sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_icon].path);
						sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_icon].title_id);
						if(strstr(menu_list[game_icon].content,"PS2")!=NULL)	sprintf(filename, "%s", ps2png);
						if(strstr(menu_list[game_icon].content,"DVD")!=NULL)	sprintf(filename, "%s", dvdpng);
						if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
						cover_available_5=0;
						sprintf(string1, "%s/%s.PNG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0) sprintf(string1, "%s/%s.JPG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0 && menu_list[game_icon].cover!=-1 && menu_list[game_icon].cover!=1) {download_cover(menu_list[game_icon].title_id, string1);
						//if(stat(string1, &s3)<0) menu_list[game_icon].cover=-1;
						}
						if(stat(string1, &s3)>=0) {
							cover_available_5=1;
							sprintf(filename,"%s",string1);
						}
						if(cover_available_5==0){
							sprintf(string1, "%s/COVER.JPG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/COVER.PNG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)>=0) {
							cover_available_5=1;
							sprintf(filename,"%s",string1);
							}
						}
//						if(game_sel_last==game_sel+1) 
//							memcpy(text_MSC_5, text_NET_6, 0x64000);
//						else
							load_texture(text_MSC_5, filename, 320);
						if(menu_list[game_icon].title[0]=='_' || menu_list[game_icon].split) gray_texture(text_MSC_5, 320, 320, 0);
					}

					game_icon=game_sel+3;
					if(game_icon<=max_menu_list) {
//						sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_icon].path);
						sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_icon].title_id);
						if(strstr(menu_list[game_icon].content,"PS2")!=NULL)	sprintf(filename, "%s", ps2png);
						if(strstr(menu_list[game_icon].content,"DVD")!=NULL)	sprintf(filename, "%s", dvdpng);
						if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
						cover_available_6=0;
						sprintf(string1, "%s/%s.PNG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0) sprintf(string1, "%s/%s.JPG", covers_dir, menu_list[game_icon].title_id);
						if(stat(string1, &s3)<0 && menu_list[game_icon].cover!=-1 && menu_list[game_icon].cover!=1) {download_cover(menu_list[game_icon].title_id, string1);
						//if(stat(string1, &s3)<0) menu_list[game_icon].cover=-1;
						}
						if(stat(string1, &s3)>=0) {
							cover_available_6=1;
							sprintf(filename,"%s",string1);
						}
						if(cover_available_6==0){
							sprintf(string1, "%s/COVER.JPG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/COVER.PNG", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)<0) sprintf(string1, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_icon].path);
							if(stat(string1, &s3)>=0) {
							cover_available_6=1;
							sprintf(filename,"%s",string1);
							}
						}
						load_texture(text_NET_6, filename, 320);
						if(menu_list[game_icon].title[0]=='_' || menu_list[game_icon].split) gray_texture(text_NET_6, 320, 320, 0);
					}

					counter_png=25;
					} 

//				if(cover_mode==5)	{	}

				if(cover_mode==6)
				{
					int a_offset=40, alpha_cbox=1, alpha_gbox=1;
					legend_y=170;

					//rnd=time(NULL)&0x03;

					sprintf(auraBG, "%s/AUR%i.JPG", app_usrdir, 4);
					load_texture(text_bmp, auraBG, 1920);

					if(game_last_page==-1)
						memcpy(text_FONT, text_bmp + (1920*4*legend_y), (1920*4*legend_h));
					last_selected=-1;
					int game_rel=0, game_rel2;//, c_x=0, c_y=0, c_game=0;
					game_rel2=int(game_sel/8)*8;
//					for (game_rel=game_rel2; (((game_rel-game_rel2)<8) && game_rel<max_menu_list); game_rel++)
					game_rel=game_sel;

//					if(game_last_page==-1)
//					{
						sprintf(filename, "%s/CBOX2.PNG", app_usrdir);
						if(stat(filename, &s3)<0) {sprintf(filename, "%s/CBOX.PNG", app_usrdir); alpha_cbox=0;}
						load_texture(text_FONT+1024*1024*1, filename, 459);

						sprintf(filename, "%s/GBOX2.PNG", app_usrdir);
						if(stat(filename, &s3)<0) {sprintf(filename, "%s/GBOX.PNG", app_usrdir); alpha_gbox=0;}
						load_texture(text_FONT+1024*1024*2, filename, 717);
//					}

					game_last_page=int(game_sel/8);

					if(alpha_cbox)
						put_texture_with_alpha( text_bmp, text_FONT+1024*1024*1, 459, 356, 459, 353, 360-a_offset, 0, 0);
					else
						put_texture_Galpha( text_bmp, 1920, 1080, text_FONT+1024*1024*1, 459, 356, 459, 353, 360-a_offset, 0, 0);

					if(alpha_gbox)
						put_texture_with_alpha( text_bmp, text_FONT+1024*1024*2, 717, 473, 717, 1112, 302-a_offset, 0, 0);
					else
						put_texture_Galpha( text_bmp, 1920, 1080, text_FONT+1024*1024*2, 717, 473, 717, 1112, 302-a_offset, 0, 0);


					sprintf(filename, "%s/%s_640.RAW", cache_dir, menu_list[game_sel].title_id);
					load_texture(text_FONT+3453716, filename, 640);
					put_texture( text_bmp, text_FONT+3453716, 640, 360, 640, 1148, 372-a_offset, 0, 0x0080ff80);

					put_reflection( text_bmp, 1920, 1080, 654, 437, 1142, 322-a_offset, 1142, 756-a_offset, 3);


					if(game_sel>0)
					{
						sprintf(filename, "%s/%s_240.RAW", cache_dir, menu_list[game_sel-1].title_id);
						load_texture(text_FONT+3453716, filename, 240);
						put_texture( text_bmp, text_FONT+3453716, 240, 135, 240, 110, 473-a_offset, 2, 0x0080ff80);
					}

					if(game_sel>1)
					{
						sprintf(filename, "%s/%s_160.RAW", cache_dir, menu_list[game_sel-2].title_id);
						load_texture(text_FONT+3453716, filename, 160);
						put_texture( text_bmp, text_FONT+3453716, 160, 90, 160, 110, 630-a_offset, 1, 0x0080ff80);
					}

					if(game_sel>2)
					{
						sprintf(filename, "%s/%s_80.RAW", cache_dir, menu_list[game_sel-3].title_id);
						load_texture(text_FONT+3453716, filename, 80);
						put_texture( text_bmp, text_FONT+3453716, 80, 45, 80, 110, 742-a_offset, 1, 0x0080ff80);
					}

					if(game_sel<max_menu_list-1)
					{
						sprintf(filename, "%s/%s_240.RAW", cache_dir, menu_list[game_sel+1].title_id);
						load_texture(text_FONT+3453716, filename, 240);
						put_texture( text_bmp, text_FONT+3453716, 240, 135, 240, 810, 473-a_offset, 2, 0x0080ff80);
					}

					if(game_sel<max_menu_list-2)
					{
						sprintf(filename, "%s/%s_160.RAW", cache_dir, menu_list[game_sel+2].title_id);
						load_texture(text_FONT+3453716, filename, 160);
						put_texture( text_bmp, text_FONT+3453716, 160, 90, 160, 890, 630-a_offset, 1, 0x0080ff80);
					}

					if(game_sel<max_menu_list-3)
					{
						sprintf(filename, "%s/%s_80.RAW", cache_dir, menu_list[game_sel+3].title_id);
						load_texture(text_FONT+3453716, filename, 80);
						put_texture( text_bmp, text_FONT+3453716, 80, 45, 80, 970, 742-a_offset, 1, 0x0080ff80);
					}

						{

						cover_available=0;
						if(strstr(menu_list[game_rel].content,"PS3")!=NULL)
						{
							sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[game_rel].title_id);
							if(stat(filename, &s3)>=0) 
							{
								cover_available=1;
								goto fixed_cover6;
							}
							else
								sprintf(filename, "%s/%s.PNG", covers_dir, menu_list[game_rel].title_id);

							if(stat(filename, &s3)>=0) 
							{
								cover_available=1;
								goto fixed_cover6;
							}
							else
							{
								if((menu_list[game_rel].cover!=-1 && menu_list[game_rel].cover!=1))
								{
									sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[game_rel].title_id);
									download_cover(menu_list[game_rel].title_id, filename);
								}
							}
							
							if(stat(filename, &s3)>=0) 
							{
								cover_available=1;
								goto fixed_cover6;
							}
							else
							{
								//menu_list[game_rel].cover=-1;
//								if(strstr(menu_list[game_rel].path, "/pvd_usb")!=NULL)
									sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_rel].title_id);
									//sprintf(filename, "%s", blankBG);
//								else
//								{
//									sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_rel].path);
									if(stat(filename, &s3)<0) sprintf(filename, "%s/ICON0.PNG", menu_list[game_rel].path);
//								}

								cover_available=0;
							}
							goto fixed_cover6;
						}

						else //not a ps3 game
						{
							if(strstr(menu_list[game_rel].content,"PS2")!=NULL)	{sprintf(filename, "%s", ps2png); goto fixed_cover6;}
							if(strstr(menu_list[game_rel].content,"DVD")!=NULL)	{
								sprintf(filename, "%s/COVER.JPG", menu_list[game_rel].path);
								if(stat(filename, &s3) < 0 ) sprintf(filename, "%s/COVER.PNG", menu_list[game_rel].path);
								if(stat(filename, &s3)>=0) {
									cover_available=1;
									goto fixed_cover6;
								}
								cover_available=0;
								sprintf(filename, "%s", dvdpng); goto fixed_cover6;
							}

							sprintf(filename, "%s/COVER.JPG", menu_list[game_rel].path);
							if(stat(filename, &s3) < 0 ) sprintf(filename, "%s/COVER.PNG", menu_list[game_rel].path);
							if(stat(filename, &s3)>=0) {
								cover_available=1;
								goto fixed_cover6;
							}

							sprintf(filename, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_rel].path);
		//					if(stat(filename, &s3)<0) sprintf(filename, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_rel].path);
							if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
fixed_cover6:
							offX=0; offY=0;
							if(cover_available==0) 
							{
								memset(text_bmpS, 0x00, 409600);
								load_texture(text_bmpS+79360, filename, 320);
							}
							else
								load_texture(text_bmpS, filename, 320);
						}

					if(menu_list[game_rel].title[0]=='_' || menu_list[game_rel].split) gray_texture(text_bmpS, 320, 320, 0);
					if(cover_available==0) 
//						put_texture( text_bmp, text_bmpS+120, 260, 176, 320, 435, 446-a_offset, 0, 0x0080ff60);
						put_texture( text_bmp, text_bmpS+120, 260, 300, 320, 435, 384-a_offset, 0, 0x0080ff60);
					else
						{
							menu_list[game_rel].cover=1;
							put_texture( text_bmp, text_bmpS, 260, 300, 320, 435, 384-a_offset, 0, 0x0080ff60);
						}

					put_reflection( text_bmp, 1920, 1080, 302, 332, 429, 378-a_offset, 429, 707-a_offset, 2);

					}

					counter_png=20; 
				}

				if(cover_mode==7 && int(game_sel/32)!=game_last_page)
				{
					legend_y=760;
					//rnd=time(NULL)&0x03;
					sprintf(auraBG, "%s/AUR5.JPG", app_usrdir);
					load_texture(text_bmp, auraBG, 1920);

					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					set_texture( text_bmp, 1920, 1080); //PIC1.PNG
					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					flip();

//					gray_texture(text_bmp, 1920, 1080);
//					if(game_last_page==-1)
					memcpy(text_FONT, text_bmp + (1920*4*legend_y), (1920*4*legend_h));
					game_last_page=int(game_sel/32); last_selected=-1;

					int game_rel=0, c_x=0, c_y=0, c_game=0, game_rel2;
					game_rel2=int(game_sel/32)*32;
					int glo_box=0;

//					sprintf(filename, "%s/CBOX.PNG", app_usrdir);
//					load_texture(text_FONT+1024*1024*7, filename, 459);
//					mip_texture( text_FONT+1024*1024*1, text_FONT+1024*1024*7+9420, 459, 356, -2);

					sprintf(filename, "%s/SBOX.PNG", app_usrdir);
					load_texture(text_FONT+1024*1024*7, filename, 432);
					mip_texture( text_FONT+1024*1024*1, text_FONT+1024*1024*7, 432, 366, -2); // -> 216x183

					sprintf(filename, "%s/GLC3.PNG", app_usrdir);
					if(stat(filename, &s3)>=0) 
					{
						load_texture(text_FONT+1024*1024*7, filename, 130);
						glo_box=1;
					}


					for (game_rel=game_rel2; (((game_rel-game_rel2)<32) && game_rel<max_menu_list); game_rel++)
						{


						cover_available=0;
						if(strstr(menu_list[game_rel].content,"PS3")!=NULL)
						{
							sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[game_rel].title_id);
							if(stat(filename, &s3)>=0) 
							{
								cover_available=1;
								goto fixed_cover_7;
							}
							else
								sprintf(filename, "%s/%s.PNG", covers_dir, menu_list[game_rel].title_id);

							if(stat(filename, &s3)>=0) 
							{
								cover_available=1;
								goto fixed_cover_7;
							}
							else
							{
								if((menu_list[game_rel].cover!=-1 && menu_list[game_rel].cover!=1))
								{
									sprintf(filename, "%s/%s.JPG", covers_dir, menu_list[game_rel].title_id);
									download_cover(menu_list[game_rel].title_id, filename);
								}
							}
							
							if(stat(filename, &s3)>=0) 
							{
								cover_available=1;
								goto fixed_cover_7;
							}
							else
							{
								//menu_list[game_rel].cover=-1;
								if(strstr(menu_list[game_rel].path, "/pvd_usb")!=NULL)
									sprintf(filename, "%s/%s_320.PNG", cache_dir, menu_list[game_rel].title_id);
									//sprintf(filename, "%s", blankBG);
								else
								{
//									sprintf(filename, "%s/PS3_GAME/ICON0.PNG", menu_list[game_rel].path);
									sprintf(filename, "%s/NOID.JPG", app_usrdir);
									cover_available=1;
									goto fixed_cover_7;
//									if(stat(filename, &s3)<0) sprintf(filename, "%s/ICON0.PNG", menu_list[game_rel].path);
								}

								cover_available=0;
							}
							goto fixed_cover_7;
						}

						else //not a ps3 game
						{
							if(strstr(menu_list[game_rel].content,"PS2")!=NULL)	{sprintf(filename, "%s", ps2png); goto fixed_cover_7;}
							if(strstr(menu_list[game_rel].content,"DVD")!=NULL)	{
								sprintf(filename, "%s/COVER.JPG", menu_list[game_rel].path);
								if(stat(filename, &s3) < 0 ) sprintf(filename, "%s/COVER.PNG", menu_list[game_rel].path);
								if(stat(filename, &s3)>=0) {
									cover_available=1;
									goto fixed_cover_7;
								}
								cover_available=0;
								sprintf(filename, "%s", dvdpng); goto fixed_cover_7;
							}

							sprintf(filename, "%s/COVER.JPG", menu_list[game_rel].path);
							if(stat(filename, &s3) < 0 ) sprintf(filename, "%s/COVER.PNG", menu_list[game_rel].path);
							if(stat(filename, &s3)>=0) {
								cover_available=1;
								goto fixed_cover_7;
							}

							sprintf(filename, "%s/HDAVCTN/BDMT_O1.jpg", menu_list[game_rel].path);
		//					if(stat(filename, &s3)<0) sprintf(filename, "%s/BDMV/META/DL/HDAVCTN_O1.jpg", menu_list[game_rel].path);
							if(stat(filename, &s3)<0) sprintf(filename, "%s", blankBG);
fixed_cover_7:
								offX=0; offY=0;
								load_texture(text_FONT+1024*1024*6, filename, 320);
								mip_texture( text_FONT+1024*1024*5, text_FONT+1024*1024*6, 320, 300, -2);
						}

					c_game=game_rel-game_rel2;

					if(c_game<8) 
					{
						c_y=62;
						c_x= 118 + (int)(216.5f*c_game);
					}

					if(c_game>7 && c_game<16) 
					{
						c_y=240;
						c_x= 118 + (int)(216.5f*(c_game-8));
					}

					if(c_game>15 && c_game<24) 
					{
						c_y=418;
						c_x= 118 + (int)(216.5f*(c_game-16));
					}

					if(c_game>23 && c_game<32) 
					{
						c_y=596;
						c_x= 118 + (int)(216.5f*(c_game-24));
					}

//mip_texture( text_FONT, text_bmpS, 320, 300, 2); //scale to 160x150
//memcpy(text_bmpS, text_FONT, 160*150*4);
//blur_texture(text_bmpS, 320, 180, 0, 0, 320, 180, 0, 0, 1);					

					if(menu_list[game_rel].title[0]=='_' || menu_list[game_rel].split) gray_texture(text_FONT+1024*1024*5, 160, 160, 0);
					if(cover_available==0)
						put_texture( text_bmp, text_FONT+1024*1024*5, 160, 88, 160, c_x+7, c_y+62, 2, 0x0080ff80);
					else
						{
							menu_list[game_rel].cover=1;

//							put_texture_Galpha( text_bmp, 1920, 1080, text_FONT+1024*1024*1, 168, 174, 230, c_x+4, c_y-10, 0, 0);
							put_texture_with_alpha( text_bmp, text_FONT+1024*1024*1, 216, 183, 216, c_x-28, c_y-16, 0, 0);
							put_texture( text_bmp, text_FONT+1024*1024*5, 130, 150, 160, c_x+15, c_y, 0, 0x0080ff60);
							if(glo_box==1) put_texture_with_alpha( text_bmp, text_FONT+1024*1024*7, 130, 150, 130, c_x+15, c_y, 0, 0);
						}

//					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
//					set_texture( text_bmp, 1920, 1080); //PIC1.PNG
//					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//					flip();

					}

					counter_png=40; 
				} //mode7

				if(cover_mode==8)
				{
					//draw column name
//					draw_xmb_icon_text(xmb_icon);
//					xmb_legend_drawn=0;
				}

			}

		}

	if(counter_png) counter_png--;

	if ((old_pad & BUTTON_START) && (new_pad & BUTTON_TRIANGLE)){
switch_ntfs:
		new_pad=0; old_pad=0;
		if(!pfs_enabled)
		{
			dialog_ret=0;
			ret = cellMsgDialogOpen2( type_dialog_yes_no, "Please attach USB storage device before you proceed!\n\nIs the disk connected to your PLAYSTATION\xC2\xAE\x33 system?", dialog_fun1, (void*)0x0000aaaa, NULL );
			wait_dialog();
			if(dialog_ret==1) {
				pfs_mode(1);
				dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
				flipc(60); sys_timer_usleep(6000*1000);
				cellMsgDialogAbort();
				forcedevices=0xFFFE;
				goto start_of_loop;
			}
		}
		else
		{
			pfs_mode(0);
			dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
			flipc(60); sys_timer_usleep(6000*1000);
			cellMsgDialogAbort();
			forcedevices=0xFFFE;
			goto start_of_loop;
		}

	}


	if ( ((old_pad & BUTTON_START) && (new_pad & BUTTON_R2)) || take_screenshot) {
		new_pad=0; old_pad=0;
		take_screenshot=0;
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		char video_mem[64];
		sprintf(video_mem, "/dev_hdd0/%04d%02d%02d-%02d%02d%02d-SCREENSHOT.RAW", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
		FILE *fpA;
		remove(video_mem);
		fpA = fopen ( video_mem, "wb" );
		uint64_t c_pos=0;
		for(c_pos=0;c_pos<video_buffer;c_pos+=4){
			fwrite((uint8_t*)(color_base_addr)+c_pos+1, 3, 1, fpA);
		}
		fclose(fpA);
		if(stat("/dev_usb000",&s3)>=0){
			sprintf(string1, "/dev_usb000/%s", video_mem+10);
			file_copy(video_mem, string1, 0);
			remove(video_mem);
			sprintf(video_mem, "%s", string1);
		}
		else
		if(stat("/dev_usb001",&s3)>=0){
			sprintf(string1, "/dev_usb001/%s", video_mem+10);
			file_copy(video_mem, string1, 0);
			remove(video_mem);
			sprintf(video_mem, "%s", string1);
		}
		sprintf(string1, "Screenshot successfully saved as:\n\n[%s]", video_mem);
		dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
	}

	if ((old_pad & BUTTON_START) && (new_pad & BUTTON_SQUARE)){
		new_pad=0; old_pad=0;
		max_mp3=0; current_mp3=0;
		//load_texture(text_bmpUPSR, playBGR, 1920);
		main_mp3((char*)"SILENCE.BIN");
	}

	 if ((old_pad & BUTTON_START) &&  (new_pad & BUTTON_RIGHT)){ //next song
		new_pad=0; old_pad=0;
		if(max_mp3!=0){
			current_mp3++;
			if(current_mp3>max_mp3 || current_mp3>=MAX_MP3) current_mp3=1;
			main_mp3((char*) mp3_playlist[current_mp3].path);
		}
	 }

	 if ((old_pad & BUTTON_START) &&  (new_pad & BUTTON_LEFT)){ //prev song
		new_pad=0; old_pad=0;
		if(max_mp3!=0){
			current_mp3--;
			if(current_mp3==0) current_mp3=max_mp3;
			main_mp3((char*) mp3_playlist[current_mp3].path);
		}
	 }

	 if ((old_pad & BUTTON_START) &&  ( (new_pad & BUTTON_DOWN) || (new_pad & BUTTON_UP)) && multiStreamStarted==1) { //mp3 volume
		if((new_pad & BUTTON_UP)) mp3_volume+=0.05f; else mp3_volume-=0.05f;
		if(mp3_volume<0.0f) mp3_volume=0.0f;
		new_pad=0; old_pad=0;
		cellMSCoreSetVolume1(nChannel, CELL_MS_DRY, CELL_MS_SPEAKER_FL, CELL_MS_CHANNEL_0, mp3_volume);
		cellMSCoreSetVolume1(nChannel, CELL_MS_DRY, CELL_MS_SPEAKER_FR, CELL_MS_CHANNEL_1, mp3_volume);
		cellMSCoreSetVolume1(nChannel, CELL_MS_DRY, CELL_MS_SPEAKER_FC,  CELL_MS_CHANNEL_0, mp3_volume);
		cellMSCoreSetVolume1(nChannel, CELL_MS_DRY, CELL_MS_SPEAKER_RL,  CELL_MS_CHANNEL_0, mp3_volume-0.1f);
		cellMSCoreSetVolume1(nChannel, CELL_MS_DRY, CELL_MS_SPEAKER_RR,  CELL_MS_CHANNEL_1, mp3_volume-0.1f);
		cellMSCoreSetVolume1(nChannel, CELL_MS_DRY, CELL_MS_SPEAKER_LFE, CELL_MS_CHANNEL_1, 0.1f);
	 }



	 if (((old_pad & BUTTON_SELECT) &&  (new_pad & BUTTON_CIRCLE)) && cover_mode!=5 && (cover_mode!=8 || (cover_mode==8 && (xmb_icon==6 || xmb_icon==7))) )
 	 {
rename_title:
		 new_pad=0; old_pad=0;
		 xmb_bg_show=0; xmb_bg_counter=200;
		 if((strstr(menu_list[game_sel].path, "/dev_hdd0")!=NULL || strstr(menu_list[game_sel].path, "/dev_usb")!=NULL) && strstr(menu_list[game_sel].content, "PS3")!=NULL)
		 {
			c_opacity_delta=16;	dimc=0; dim=1;
			OutputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
			OutputInfo.numCharsResultString = 64;
			OutputInfo.pResultString = Result_Text_Buffer;
			open_osk(4, (menu_list[game_sel].title[0]=='_' ? menu_list[game_sel].title+1 : menu_list[game_sel].title) );
			if(cover_mode!=8)
			 {
				sprintf(filename, "%s/%s_1920.PNG", cache_dir, menu_list[game_sel].title_id);
				if(stat(filename, &s3)<0) sprintf(filename, "%s", avchdBG);
				load_texture( text_FONT, filename, 1920);// gray_texture(text_FONT, 1920, 1080);

				max_ttf_label=0;
				sprintf(string1, "::: %s :::", (menu_list[game_sel].title[0]=='_' ? menu_list[game_sel].title+1 : menu_list[game_sel].title));
				print_label_ex( 0.5f, 0.10f, 1.0f, 0xffffffff, string1, 1.04f, 0.0f, 1, 1.0f, 1.0f, 1);
				flush_ttf(text_FONT, 1920, 1080);
				max_ttf_label=0;
				sprintf(string1, "%s", "Enter new game title:");
				print_label_ex( 0.5f, 0.20f, 1.2f, 0xffffffff, string1, 1.00f, 0.0f, 2, 1.0f, 1.0f, 1);
				flush_ttf(text_FONT, 1920, 1080);
			 }

			while(1){ 
				{
					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					if(cover_mode!=8)
						{set_texture( text_FONT, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080); }
					else
						draw_whole_xmb(0);
					setRenderColor();
					flip(); 
				}

				//cellSysutilCheckCallback();
				if(osk_dialog==1 || osk_dialog==-1) break;
			}

			osk_open=0;
			if(osk_dialog!=0)
			{
				char pin_result[128];
				wchar_t *pin_result2;
				pin_result2 = (wchar_t*)OutputInfo.pResultString;
				wcstombs(pin_result, pin_result2, 64);
				if(strlen(pin_result)>1) {
					u8 n;
					for(n=0; n<(strlen(pin_result)-3); n++)
					{
						// (TM) = E2 84 A2 | 28 54 4D 29  
						if(pin_result[n]==0x28 && (pin_result[n+1]==0x54 || pin_result[n+1]==0x74) && (pin_result[n+2]==0x4D || pin_result[n+2]==0x6D) && pin_result[n+3]==0x29)
						{
							pin_result[n]  =0xE2;
							pin_result[n+1]=0x84;
							pin_result[n+2]=0xA2;
							// pin_result[n+3]=0x20;
							strncpy(pin_result+n+3, pin_result+n+4, strlen(pin_result)-n-4); pin_result[strlen(pin_result)-1]=0;
						}
					}

					for(n=0; n<(strlen(pin_result)-2); n++)
					{
						// (R)  = C2 AE    | 28 52 29 
						if(pin_result[n]==0x28 && (pin_result[n+1]==0x52 || pin_result[n+1]==0x72) && pin_result[n+2]==0x29)
						{
							pin_result[n]  =0xC2;
							pin_result[n+1]=0xAE;
							// pin_result[n+2]=0x20;
							// test(r): abc
							// 01234567890123
							strncpy(pin_result+n+2, pin_result+n+3, strlen(pin_result)-n-3); pin_result[strlen(pin_result)-1]=0;
						}
					}

					for(n=0; n<(strlen(pin_result)-2); n++)
					{
						// (C)  = C2 A9    | 28 63 29
						if(pin_result[n]==0x28 && (pin_result[n+1]==0x43 || pin_result[n+1]==0x63) && pin_result[n+2]==0x29)
						{
							pin_result[n]  =0xC2;
							pin_result[n+1]=0xA9;
							strncpy(pin_result+n+2, pin_result+n+3, strlen(pin_result)-n-3); pin_result[strlen(pin_result)-1]=0;
						}
					}
					sprintf(filename, "%s/PS3_GAME/PARAM.SFO", menu_list[game_sel].path);
					if(stat(filename, &s3)<0) sprintf(filename, "%s/PARAM.SFO", menu_list[game_sel].path);
					change_param_sfo_field( filename, (char*)"TITLE", pin_result);
					sprintf(menu_list[game_sel].title, "%s", pin_result);
					}
			}
			if(cover_mode==3) load_texture(text_FONT, userBG, 1920);
			if(cover_mode==8) {xmb[6].init=0; xmb[7].init=0; init_xmb_icons(menu_list, max_menu_list, game_sel );}
			game_last_page=-1; old_fi=-1;
			goto force_reload;
		 }
	}

	if ((old_pad & BUTTON_SELECT) && (new_pad & BUTTON_R1)){
		new_pad=0; old_pad=0;
		c_opacity_delta=16;	dimc=0; dim=1;
		display_mode++;
		if(display_mode>2) display_mode=0;
		if(cover_mode==8) redraw_column_texts(xmb_icon);
		old_fi=-1;
		counter_png=0;
		forcedevices=0xFFFF;
		max_menu_list=0;

		game_last_page=-1;
		goto start_of_loop;
//		uint64_t ret2=0x00ULL;
//		ret2 = syscall_838("/dev_bdvd");
//		ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_SIMPLE", "/dev_bdvd", 0, 1, 0, 0, 0);
//		delete_entries(menu_list, &max_menu_list, 1);
//		game_sel=0;
	}


	 if ( (new_pad & BUTTON_R1) && (lock_display_mode==-1) ) {// && cover_mode!=5
next_for_FM:
//	 	if(multiStreamStarted!=0) { cellMSStreamClose(nChannel); cellMSCoreStop(nChannel, 0);}
		c_opacity_delta=16;	dimc=0; dim=1;
//		memset(text_bmp, 0x00, 1920*1080*4);
		if(cover_mode==3) {slide_screen_left(text_FONT); load_texture(text_bmpUPSR, playBGR, 1920);} else
		if(cover_mode==4 || cover_mode==5) slide_screen_left(text_bmpUPSR); else slide_screen_left(text_bmp);
		last_cover_mode=cover_mode;
//change_display_mode:
		if(cover_mode==5) 
		{
			load_texture(text_bmpUPSR, playBGR, 1920);
			if(lock_display_mode!=-1) cover_mode=lock_display_mode-1;

		}
		cover_mode++; if(cover_mode==5) cover_mode++;
		c_opacity=0xff; c_opacity2=0xff;
		game_last_page=-1;
		game_sel_last=game_sel;
		new_pad=0;
		is_sliding=0;

		state_read=1;
		state_draw=1;

		if(cover_mode>8) {cover_mode=0;}
		if(cover_mode!=4) {
				load_texture(text_BLU_1, iconBLU, 320);
				load_texture(text_NET_6, iconNET, 320);
				load_texture(text_OFF_2, iconOFF, 320);
				load_texture(text_CFC_3, iconCFC, 320);
				load_texture(text_SDC_4, iconSDC, 320);
				load_texture(text_MSC_5, iconMSC, 320);
		}
		if(cover_mode==3) load_texture(text_FONT, userBG, 1920);
		if(cover_mode==5) set_fm_stripes();
		if(cover_mode<3 || cover_mode>5)  load_texture(text_FMS, legend, 1665);//&& last_cover_mode>2)
		if(cover_mode==8) {init_xmb_icons(menu_list, max_menu_list, game_sel );}
		old_fi=-1;
		counter_png=0;
		goto force_reload;
		}

//	 if ((old_pad & BUTTON_SELECT) &&  (new_pad & BUTTON_L1) &&  (lock_display_mode==-1 || cover_mode==5) ) {
	 if ( ((new_pad & BUTTON_L1) || ((old_pad & BUTTON_SELECT) &&  (new_pad & BUTTON_L1))) &&  (lock_display_mode==-1 || cover_mode==5) ) {
		//if(cover_mode==5) goto next_for_FM;
		c_opacity_delta=16;	dimc=0; dim=1;
		if(cover_mode==3) slide_screen_right(text_FONT); else
		if(cover_mode==4 || cover_mode==5) slide_screen_right(text_bmpUPSR); else slide_screen_right(text_bmp);

		if(cover_mode==5 || cover_mode==6) {
			load_texture(text_bmpUPSR, playBGR, 1920);
			load_texture(text_FMS, legend, 1665);
			sprintf(auraBG, "%s/AUR5.JPG", app_usrdir);
			load_texture(text_bmp, auraBG, 1920);
			game_last_page=-1;
			if((old_pad & BUTTON_SELECT))
			{
				if(last_cover_mode==5)
					cover_mode--;
				else
					cover_mode=last_cover_mode;
			}
			else
			{
				last_cover_mode=cover_mode;
				cover_mode--;
			}
		}
		else
		 {

			last_cover_mode=cover_mode;
			cover_mode--;
			game_last_page=-1;
		 }
		 if(cover_mode==5) cover_mode--;
		if(lock_display_mode!=-1) cover_mode=lock_display_mode;
		game_sel_last=game_sel;
		state_read=1;
		state_draw=1;
		is_sliding=0;
		new_pad=0; old_pad=0;
		c_opacity=0xff; c_opacity2=0xff;

		if(cover_mode<0) {cover_mode=8;}
		if(cover_mode!=4) {
				load_texture(text_BLU_1, iconBLU, 320);
				load_texture(text_NET_6, iconNET, 320);
				load_texture(text_OFF_2, iconOFF, 320);
				load_texture(text_CFC_3, iconCFC, 320);
				load_texture(text_SDC_4, iconSDC, 320);
				load_texture(text_MSC_5, iconMSC, 320);
		}
		if(cover_mode==3) load_texture(text_FONT, userBG, 1920);
		if(cover_mode==5) set_fm_stripes();
		if((cover_mode<3 || cover_mode>5) && last_cover_mode>2)  load_texture(text_FMS, legend, 1665);
		if(cover_mode==8) {init_xmb_icons(menu_list, max_menu_list, game_sel );}
		old_fi=-1;
		counter_png=0;
		goto force_reload;
		}


	    if ( (old_pad & BUTTON_SELECT) && (new_pad & BUTTON_START))
		{	
			if(cover_mode==5) new_pad=0;
open_file_manager:

			state_draw=1; c_opacity=0xff; c_opacity2=0xff;
			if(!lock_fileman)
			{
				if(cover_mode!=5) last_cover_mode=cover_mode;			
				cover_mode=5;
				counter_png=0;
				state_read=1;
				state_draw=1;
				memset(text_bmpUPSR, 0, 8294400);
				//new_pad=0;// old_pad=0;

				load_texture(text_BLU_1, iconBLU, 320);
				load_texture(text_NET_6, iconNET, 320);
				load_texture(text_OFF_2, iconOFF, 320);
				load_texture(text_CFC_3, iconCFC, 320);
				load_texture(text_SDC_4, iconSDC, 320);
				load_texture(text_MSC_5, iconMSC, 320);
				set_fm_stripes();
				old_fi=-1;
			}
		}

	    if (( (old_pad & BUTTON_L2) && (old_pad & BUTTON_R2)) || (ss_timer>=(ss_timeout*60) && ss_timeout) || www_running) screen_saver();

	    if ( (old_pad & BUTTON_START) && (new_pad & BUTTON_SELECT))
		{	
			char reload_self[128];
			//new_pad=0; old_pad=0;
			sprintf(reload_self, "%s/RELOAD.SELF", app_usrdir);
			if(stat(reload_self,&s3)>=0 && net_used_ignore())
			{
				cellMsgDialogAbort();
				dialog_ret=0; cellMsgDialogOpen2( type_dialog_yes_no, "Restart multiMAN?", dialog_fun1, (void*)0x0000aaaa, NULL ); wait_dialog();
				
				if(dialog_ret==1){

					unload_modules();
					sys_game_process_exitspawn2((char*) reload_self, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);	
				}
			}
		}

	if(force_update_check==1) {check_for_update(); force_update_check=0;}
	if(cover_mode==5) {goto skip_to_FM;}	

skip_find_device:
	is_game_loading=0;
	if(read_pad_info()) goto force_reload;

    // update the game folder


/*	 if ( (new_pad & BUTTON_L2) && 0){		
//open_setup:
 		new_pad=0; old_pad=0;
		dir_fixed=0; parse_ini(options_ini,1);
		parse_color_ini();
		load_texture(text_BLU_1, iconBLU, 320);
		//if(cover_mode<3 || cover_mode>5)  load_texture(text_FMS, legend, 1665); else load_texture(text_FMS, playBG, 1920);
		game_last_page=-1; old_fi=-1;
		goto force_reload;
		}
*/

/*	 if ( new_pad & BUTTON_START){
		game_sel=0;
		mode_list^=1;
		old_fi=-1;
		counter_png=0;
		cover_mode=0;
		} */

	if ((old_pad & BUTTON_SELECT) && (new_pad & BUTTON_L3)){
refresh_list:
		new_pad=0; is_reloaded=0;
		//sys_timer_usleep(1*1000*1000);
		c_opacity_delta=16;	dimc=0; dim=1;
		old_fi=-1;
		counter_png=0;
		forcedevices=0xFFFF;
		max_menu_list=0;
		reset_xmb(1);
		//if(cover_mode==8) reset_xmb(1);
		goto start_of_loop;
//		uint64_t ret2=0x00ULL;
//		ret2 = syscall_838("/dev_bdvd");
//		{sprintf(filename, "Error (%08X) occured while unmounting /dev_bdvd!", (u32)ret2); dialog_ret=0; ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();}
//		ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_UDF", "/dev_ps2disc", 0, 1, 0, 0, 0);
//		ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_SIMPLE", "/dev_bdvd", 0, 1, 0, 0, 0);
//		ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_UDF", "/dev_bdvd", 0, 1, 0, 0, 0);
//		{sprintf(filename, "Error (%08X) occured while mounting /dev_bdvd!", (u32)ret2); dialog_ret=0; ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();} 
		//game_sel=0;
	}

	 if ( new_pad & BUTTON_L3){
  	    dir_mode++;
		new_pad=0;
		if(dir_mode>2) dir_mode=0;
		if(cover_mode==8) redraw_column_texts(xmb_icon);
		old_fi=-1;
		counter_png=0;
		c_opacity_delta=16;	dimc=0; dim=1;
		goto force_reload;
		}

	 if ( (new_pad & BUTTON_R3)) {
		new_pad=0;//new_pad=0; 
		user_font++; if (user_font>9) user_font=0;
		if(cover_mode==8) redraw_column_texts(xmb_icon);
		old_fi=-1;
		counter_png=0;
		game_last_page=-1;
		goto force_reload;
			
	} 

	 if ( (old_pad & BUTTON_START) && (new_pad & BUTTON_R3)){
		new_pad=0;
update_title:
		if(strstr(menu_list[game_sel].content, "PS3")!=NULL)
			check_for_game_update(menu_list[game_sel].title_id, menu_list[game_sel].title);
	} 



	 if ( (new_pad & BUTTON_TRIANGLE) && cover_mode!=8) 
		{
			if(cover_mode==4) {	sprintf(auraBG, "%s/AUR5.JPG", app_usrdir); load_texture(text_bmp, auraBG, 1920);}
			int ret_f=open_mm_submenu(text_bmp);//, &game_sel);
			if(ret_f) {slide_screen_left(text_FONT);memset(text_bmp, 0, 1920*1080*4);}
			if(cover_mode==8 || ret_f==11) 
			{	
				load_texture(text_FMS, xmbicons, 128);	memset(text_bmp, 0, 8294400); load_texture(text_bmp, xmbbg, 1920);
				if(ret_f==11) {cover_mode=8; xmb_icon=2; init_xmb_icons(menu_list, max_menu_list, game_sel ); }
			}
			if(cover_mode==3) load_texture(text_FONT, userBG, 1920);		 
			old_fi=-1;

			if(ret_f==1) force_update_check=1; 
			if(ret_f==2) {	if(!lock_fileman) {last_cover_mode=cover_mode; cover_mode=5; new_pad=0; goto open_file_manager;}}
			if(ret_f==3) goto switch_ntfs; 
//			if(ret_f==4) {take_screenshot=1;}
			
			if(ret_f==5 && net_used_ignore()) {
				char reload_self[128];
				sprintf(reload_self, "%s/RELOAD.SELF", app_usrdir);
				if(stat(reload_self,&s3)>=0)
				{
					unload_modules();
					sys_game_process_exitspawn2((char*) reload_self, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);	
				} 
			}
			if(ret_f==6 && net_used_ignore()) {unload_modules(); sys_process_exit(1);}

			if(ret_f==7) goto refresh_list;
			if( (ret_f==8 || ret_f==9)  && net_used_ignore() ) {
				sprintf(my_mp3_file, "%s/XMB Video", app_usrdir);
				if(stat(my_mp3_file, &s3)<0 || ret_f==9)
				{
					if(stat(my_mp3_file, &s3)<0) mkdir(my_mp3_file, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
					else del_temp(my_mp3_file);
					max_dir_l=0;
					ps3_home_scan_bare2((char*)"/dev_hdd0/video", pane_l, &max_dir_l); 
					char linkfile[512];
					for(ret_f=0; ret_f<max_dir_l; ret_f++)
					{
						sprintf(filename, "%s/XMB Video/%s", app_usrdir, pane_l[ret_f].name);
						sprintf(linkfile, "%s/%s", pane_l[ret_f].path, pane_l[ret_f].name);
						link(linkfile, filename);
					}

				}
//						sprintf(filename,  "/dev_hdd0/game/%s/ICON0.PNG", app_path);
//						sprintf(string1, "%s/XMB Video/-XMB virtual Video folder.PNG", app_usrdir);
//						file_copy(filename, string1, 0);
retry_showtime_mm:
						sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
						if(stat(filename, &s3)>=0)
						{
							
/*							FILE *flist; 
							sprintf(string1, "%s/TEMP/SHOWTIME.TXT", app_usrdir);
							remove(string1);
							flist = fopen(string1, "w");
							sprintf(filename, "file://%s/XMB Video", app_usrdir);fputs (filename,  flist ); 
							fclose(flist); 
*/
//							pad_read();
							unload_modules();
							sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
							sys_game_process_exitspawn2(filename, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);

/*							char* launchenvp[2];
							char* launchargv[1];
							memset(launchenvp, 0, sizeof(launchenvp));
							memset(launchargv, 0, sizeof(launchargv));
							launchenvp[0] = (char*)malloc(0x440);
							snprintf(launchenvp[0], 0x440, "showtime=%s", my_mp3_file);
							char args[512]; sprintf(args, "file://%s/XMB Video/", app_usrdir);
							int argslen=strlen(args)+1;

							int pos = 0;
							int i = 0;
							while (pos < argslen) {
								int len = strlen((char*)args + pos);
								if (!len)
									break;
								launchargv[i] = (char*)malloc(len + 1);
								strcpy(launchargv[i], (char*)args + pos);
								pos += len + 1;
								i++;
							}
							//unload_modules();
							//sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
							//sys_game_process_exitspawn2(filename, (const char**)launchargv, (const char**)launchenvp, NULL, 0, 1001, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
*/
						}
						else
						{
							cellMsgDialogAbort();
							dialog_ret=0;
							sprintf(filename, "Showtime for multiMAN application is missing!\n\nDo you want to download it now?");
							cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
							wait_dialog();
							if(dialog_ret==1)
							{
								sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
								sprintf(string1, "%s/SHOWTIME.SELF", url_base);
								download_file(string1, filename, 1);
								goto retry_showtime_mm;
							}
						}

			}

			if(ret_f==10) screen_saver();
//			if(ret_f==11) goto open_setup;
			if(ret_f==12 && stat(helpMME, &s3)>=0 && net_used_ignore())
			{
				unload_modules();
				sys_game_process_exitspawn2((char*) helpMME, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);	
			}
			goto force_reload;
		}


	if ((old_pad & BUTTON_R1) && c_firmware!=3.41f && 0) 
		{
setperm_title:
			dialog_ret=0; cellMsgDialogOpen2( type_dialog_back, "Setting access permissions, please wait...", dialog_fun2, (void*)0x0000aaab, NULL );	flipc(62);
			sprintf(filename, "%s", menu_list[game_sel].path);
			abort_rec=0;
			fix_perm_recursive(filename);
			new_pad=0; old_pad=0;
			cellMsgDialogAbort(); flip();
		}
/*
	if ((old_pad & BUTTON_R1) && c_firmware==3.41f)
	{	

		old_pad=0;
		c_opacity_delta=16;	dimc=0; dim=1;
		if(peekq(0x80000000000505d0ULL) == memvalnew || patchmode == -1)
		{
			pokeq(0x80000000000505d0ULL, memvaloriginal);
			patchmode = 0;
		}
		else
		{
			pokeq(0x80000000000505d0ULL, memvalnew);
			patchmode = 1;
		}
			//reset game list
			old_fi=-1;
			counter_png=0;
			forcedevices=0xffff;
			max_menu_list=0;
			delete_entries(menu_list, &max_menu_list, 1);
//			game_sel=0;
	}
	*/
	/*
	if ((new_pad & BUTTON_R2) && 0)
		{
			new_pad=0;
			if(stat(helpMME, &s3)>=0)
			{
				unload_modules();
				sys_game_process_exitspawn2((char*) helpMME, NULL, NULL, NULL, 0, 1001, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);	
			}	
		} */

	if ((new_pad & BUTTON_R2) && game_sel>=0 && max_menu_list>0 && mode_list==0 && 0)
	{
test_title:
		time_start= time(NULL);
			
		abort_copy=0;

		initConsole();

		file_counter=0;
		new_pad=0;
		
		global_device_bytes=0;

		num_directories= file_counter= num_files_big= num_files_split= 0;

				sprintf(string1,"Checking, please wait...\n\n%s",menu_list[game_sel].path);

				cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
				draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
				cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
				cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Hold /\\ to Abort");
				cellDbgFontDrawGcm();
				flip(); 

//		if(strstr(menu_list[game_sel].path,"/dev_hdd0")!=NULL)
				my_game_test( menu_list[game_sel].path, 0);
//			else
//				my_game_test( menu_list[game_sel].path, 1);

		DPrintf("Directories: %i Files: %i\nBig files: %i Split files: %i\n\n", num_directories, file_counter, num_files_big, num_files_split);

		int seconds= (int) (time(NULL)-time_start);
		int vflip=0;

		while(1){

			if(abort_copy==2) sprintf(string1,"Aborted!  Time: %2.2i:%2.2i:%2.2i\n", seconds/3600, (seconds/60) % 60, seconds % 60);
			else
				if(abort_copy==1)
				sprintf(string1,"Folder contains over %i files. Time: %2.2i:%2.2i:%2.2i Vol: %1.2f GB+\n", file_counter, seconds/3600, (seconds/60) % 60, seconds % 60, ((double) global_device_bytes)/(1024.0*1024.*1024.0));
				else
				sprintf(string1,"Files tested: %i Time: %2.2i:%2.2i:%2.2i Size: %1.2f GB\nActual size : %.f bytes", file_counter, seconds/3600, (seconds/60) % 60, seconds % 60, ((double) global_device_bytes)/(1024.0*1024.*1024.0),(double) global_device_bytes);
			

			cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
			draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x10101080);
			cellDbgFontPrintf( 0.07f, 0.07f, 1.2f,0xc0c0c0c0,string1);
			
			if(vflip & 32)
			cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xffffffff, "Press [ ] to continue");
			vflip++;

			cellDbgFontDrawGcm();
			flip();
			pad_read();
			if (new_pad & BUTTON_SQUARE) 
				{
				new_pad=0;
				break;
				}

			}
		termConsole();
			
		}
 
// delete from devices	

	if ( (new_pad & BUTTON_SQUARE) && game_sel>=0 && max_menu_list>0 && mode_list==0 && (!(menu_list[game_sel].flags & 2048)) && disable_options!=1 && disable_options!=3 && strstr(menu_list[game_sel].path,"/pvd_usb")==NULL && 0){
delete_title:
		int n;
		c_opacity_delta=16;	dimc=0; dim=1;
			for(n=0;n<11;n++){
				if((menu_list[game_sel].flags>>n) & 1) break;
				}

			if(n==0)
				sprintf(filename, "Do you want to delete title from internal HDD?"); 
			else
				sprintf(filename, "Do you want to delete title from external USB00%c?", 47+n); 

				dialog_ret=0;
				ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				wait_dialog();
				
			if(dialog_ret==1){

				sprintf(filename, "%s/%s", game_cache_dir, menu_list[game_sel].title_id);
				if(stat(filename, &s3)>=0)
				{
					dialog_ret=0;
					ret = cellMsgDialogOpen2( type_dialog_yes_no, "There is cached data for this title. Do you want to clear it?", dialog_fun1, (void*)0x0000aaaa, NULL );
					wait_dialog();
				}

				if(dialog_ret==1){
					sprintf(filename, "%s/%s", game_cache_dir, menu_list[game_sel].title_id);
					my_game_delete(filename);
				}

				time_start= time(NULL);
				
		
				old_fi=-1;
				counter_png=0;
				game_last_page=-1;
				forcedevices=(1<<n);
				
				abort_copy=0;
				initConsole();
				file_counter=0;
				new_pad=0;
				
				DPrintf("Deleting... \n %s\n\n", menu_list[game_sel].path);

				my_game_delete((char *) menu_list[game_sel].path);
				
				rmdir((char *) menu_list[game_sel].path); // delete this folder

				if(game_sel>max_menu_list-1) game_sel=max_menu_list-1;

				int seconds= (int) (time(NULL)-time_start);
				int vflip=0;

				while(1){

					if(abort_copy) sprintf(string1,"Aborted!  Time: %2.2i:%2.2i:%2.2i\n", seconds/3600, (seconds/60) % 60, seconds % 60);
					else
					{sprintf(string1,"Done!  Files Deleted: %i Time: %2.2i:%2.2i:%2.2i\n", file_counter, seconds/3600, (seconds/60) % 60, seconds % 60); break;}

					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

//					draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x200020ff);
					cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
					
					if(vflip & 32)
						cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Press [ ] to continue.");

					vflip++;

					cellDbgFontDrawGcm();
					flip();

					pad_read();
					if (new_pad & BUTTON_SQUARE) 
						{
						new_pad=0;
						break;
						}

				}
			termConsole();
			}
		}

// copy from devices

	if ((new_pad & BUTTON_CIRCLE) && game_sel>=0 && max_menu_list>0 && mode_list==0 && disable_options!=2 && disable_options!=3 && 0)//  && !patchmode
		{
copy_title:
		c_opacity_delta=16;	dimc=0; dim=1;
		if(menu_list[game_sel].flags & 2048) goto copy_from_bluray;

		int n;
		int curr_device=0;
		char name[1024];
		int dest=0;

		dialog_ret=0;
		if(menu_list[game_sel].flags & 1) // is hdd0
			{
				
				for(n=1;n<11;n++)
				{
				dialog_ret=0;
				
				if((fdevices>>n) & 1)
					{

					sprintf(filename, "Do you want to copy game from internal HDD to external USB00%c?", 47+n); 
					ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
					wait_dialog();

					if(dialog_ret==1)  {curr_device=n;break;} // exit
					}
				}
			
		  
		   dest=n;
           if(dialog_ret==1)
				{
				char *p=gameID;
				char *pch=menu_list[game_sel].path;
				int len = strlen(pch), i;
				char *pathpos=strrchr(pch,'/');	int lastO=pathpos-pch+1;
				for(i=lastO;i<len;i++)p[i-lastO]=pch[i];p[i-lastO]=0;

//				fix_perm_recursive(ini_usb_dir);
				sprintf(name, "/dev_usb00%c/%s", 47+curr_device, ini_usb_dir);
				mkdir(name, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
				sprintf(name, "/dev_usb00%c/%s/%s", 47+curr_device, ini_usb_dir, p);

				if(stat(name, &s3)==0)
				{
					sprintf(string1, "Destination already contains folder with the same name!\n\nPlease use FILE MANAGER [SELECT+START] to rename or remove:\n\n[%s]", name );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
					goto overwrite_cancel;
				}

				mkdir(name, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
					
				}

			}
		else
		if(fdevices & 1)
			{

				for(n=1;n<13;n++)
					{
					if((menu_list[game_sel].flags>>n) & 1) break;
					}

				if(n==11 || n==12) continue;

				curr_device=0;
				
				dest=0;
				char *p=gameID;
				char *pch=menu_list[game_sel].path;
				int len = strlen(pch), i;
				char *pathpos=strrchr(pch,'/');	int lastO=pathpos-pch+1;
				for(i=lastO;i<len;i++)p[i-lastO]=pch[i];p[i-lastO]=0;
				if(p[0]=='_') p++; // skip special char

				dialog_ret=0;
				if(force_disable_copy==0) {
					if(strstr(menu_list[game_sel].path,"/pvd_usb")==NULL)
						sprintf(filename, "Do you want to copy game from external USB00%c to internal HDD?\n\nSource: /dev_usb00%i/%s/%s\nDestination: %s/%s", 47+n, n-1, ini_usb_dir, p, hdd_folder, p); 
					else
						sprintf(filename, "Do you want to copy game from external USB disk to internal HDD?\n\nSource: %s\nDestination: %s/%s", menu_list[game_sel].path, hdd_folder, p); 
					ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
					wait_dialog();
				}

				if(dialog_ret==1)
				{
					
					mkdir(hdd_folder, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
					//fix_perm_recursive(hdd_folder);
					sprintf(name, "%s/%s", hdd_folder, p);	
					if(stat(name, &s3)==0)
					{
						sprintf(string1, "Destination already contains folder with the same name!\n\nPlease use FILE MANAGER [SELECT+START] to rename or remove:\n\n[%s]", name );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
						goto overwrite_cancel;
					}
					mkdir(name, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

				}
				else //ext to ext (USB2USB)
				{

					for(int n3=1;n3<11;n3++)
					{
						dialog_ret=0;
						if( ((fdevices>>n3) & 1) && n3!=n )
						{
							sprintf(filename, "Do you want to copy game from external USB00%c to external USB00%c?\n\nSource: /dev_usb00%i/%s/%s\nDestination: /dev_usb00%i/%s/%s", 47+n, 47+n3, n-1, ini_usb_dir, p, n3-1, ini_usb_dir, p); 
							ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
							wait_dialog();

				if(dialog_ret==1)
				{
					
					sprintf(string1, "/dev_usb00%c/%s", 47+n3, ini_usb_dir);
					mkdir(string1, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
					sprintf(name, "/dev_usb00%c/%s/%s", 47+n3, ini_usb_dir, p);
					if(stat(name, &s3)==0)
					{
						sprintf(string1, "Destination already contains folder with the same name!\n\nPlease use FILE MANAGER [SELECT+START] to rename or remove:\n\n[%s]", name );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
						goto overwrite_cancel;
					}
					mkdir(name, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
					dest=n3; curr_device=n3;
					break;
				}

						}
					}

				}
			
			}
		
		if(dialog_ret==1)
			{

				old_fi=-1;
				counter_png=0;
				game_last_page=-1;
				forcedevices=(1<<curr_device);
				time_start= time(NULL);
	
				abort_copy=0;
				initConsole();
				file_counter=0;
				new_pad=0;

				DPrintf("Copying... \n %s\n to %s\n\n", menu_list[game_sel].path, name);

				if(curr_device!=0) copy_mode=1; // break files >= 4GB
						else copy_mode=0;

				copy_is_split=0;

				num_directories= file_counter= num_files_big= num_files_split= 0;
				my_game_copy((char *) menu_list[game_sel].path, (char *) name); //ret=cellMsgDialogAbort();sys_timer_usleep(100000);

				cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

				int seconds= (int) (time(NULL)-time_start);
				int vflip=0;
				
				if(copy_is_split && !abort_copy)
					{
					char *p=gameID;
					char *pch=menu_list[game_sel].path;
					int len = strlen(pch), i;
					char *pathpos=strrchr(pch,'/');	int lastO=pathpos-pch+1;
					for(i=lastO;i<len;i++)p[i-lastO]=pch[i];p[i-lastO]=0;
					if(p[0]=='_') p++; // skip special char

					if(dest==0) 
						sprintf(filename, "%s/_%s", hdd_folder, p);
					else
						sprintf(filename, "/dev_usb00%c/%s/_%s", 47+dest, ini_usb_dir, p);


					// try rename
					ret=rename(name, filename);

					/*
					if(dest==0)   sprintf(filename, "%s\n\nSplit game copied to internal HDD (non bootable)", menu_list[game_sel].title);
					else
						sprintf(filename, "%s\n\nSplit game copied to external USB00%c (non bootable)", menu_list[game_sel].title, 47+curr_device);
					
					dialog_ret=0;

					ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );					
					
					wait_dialog();
					*/

					}

				while(1)
				{
					if(abort_copy) sprintf(string1,"Aborted! (%i)  Time: %2.2i:%2.2i:%2.2i\n", abort_copy, seconds/3600, (seconds/60) % 60, seconds % 60);
					else
						{
						if(use_symlinks==1)
							{
								sprintf(filename, "%s/USRDIR/EBOOT.BIN", menu_list[game_sel].path);
								sprintf(string1 , "%s/USRDIR/MM_EBOOT.BIN", name);
								file_copy(filename, string1, 0);
							}
						sprintf(string1,"Done! Files Copied: %i Time: %2.2i:%2.2i:%2.2i Vol: %1.2f GB\n", file_counter, seconds/3600, (seconds/60) % 60, seconds % 60, ((double) global_device_bytes)/(1024.0*1024.*1024.0));
						}

				
					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
//					draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x200020ff);

					cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
					
					if(vflip & 32)
					      cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Press [ ] to continue");

					vflip++;

					cellDbgFontDrawGcm();

					flip();

					pad_read();
					if (new_pad & BUTTON_SQUARE) 
						{
						new_pad=0;
						break;
						}

				}
		
			if(abort_copy )
				{
					if(dest==0)   sprintf(filename, "%s\n\nDelete partial copy from internal HDD?", menu_list[game_sel].title);
						else sprintf(filename, "%s\n\nDelete partial copy from USB00%c?", menu_list[game_sel].title, 47+dest);

					dialog_ret=0;
					ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
							
					wait_dialog();

					if(dialog_ret==1)
						{

						abort_copy=0;
						time_start= time(NULL);
						file_counter=0;

						my_game_delete((char *) name);
						rmdir((char *) name); // delete this folder

						if(game_sel>max_menu_list-1) game_sel=max_menu_list-1;

						}
					else
						{
					char *p=gameID;
					char *pch=menu_list[game_sel].path;
					int len = strlen(pch), i;
					char *pathpos=strrchr(pch,'/');	int lastO=pathpos-pch+1;
					for(i=lastO;i<len;i++)p[i-lastO]=pch[i];p[i-lastO]=0;
					if(p[0]=='_') p++; // skip special char
						if(dest==0) 
							sprintf(filename, "%s/_%s", hdd_folder, p);	
						else
							sprintf(filename, "/dev_usb00%c/%s/_%s", 47+dest, ini_usb_dir, p);
						
						ret=rename(name, filename);
						}
				}

			if(game_sel>max_menu_list-1) game_sel=max_menu_list-1;
			termConsole();
			}
	
		}

overwrite_cancel:
// copy from bluray

	    if ( (new_pad & BUTTON_CIRCLE) & ((fdevices>>11) & 1) && mode_list==0 && disable_options!=2 && disable_options!=3)
		{
copy_from_bluray:
		c_opacity_delta=16;	dimc=0; dim=1;
		char name[1024];
		int curr_device=0;
//		CellFsStat status;
		char id2[16], id[16];
		
		int n;

		for(n=0;n<11;n++)
			{
			dialog_ret=0;
			
			if((fdevices>>n) & 1)
				{

				if(n==0) sprintf(filename, "%s", "Do you want to copy game from BD-ROM to internal HDD?"); 
				else sprintf(filename, "Do you want to copy game from BD-ROM to external USB00%c?", 47+n); 

				ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				
				wait_dialog();
	
				if(dialog_ret==1)  {curr_device=n;break;} // exit
				}
			}
			
         if(dialog_ret==1)
			{
				

				if(curr_device==0) sprintf(name, "/dev_hdd0");
				else sprintf(name, "/dev_usb00%c", 47+curr_device);

				
				if (cellFsStat(name, &status) == CELL_FS_SUCCEEDED && !parse_ps3_disc((char *) "/dev_bdvd/PS3_DISC.SFB", id2))
					{
					
					// reset to update datas
					//game_sel=0;
					old_fi=-1;
					counter_png=0;
					forcedevices=(1<<curr_device);
					int gn, gn2=0; char bluray_game2[128]; bluray_game2[0]=0;
					for(gn=0; (gn<(int)strlen(bluray_game) && gn<53 && gn2<53); gn++ )
					{
						if( (bluray_game[gn]>0x2f && bluray_game[gn]<0x3a) || (bluray_game[gn]>0x60 && bluray_game[gn]<0x7b) || (bluray_game[gn]>0x40 && bluray_game[gn]<0x5b) || bluray_game[gn]==0x20)
						{
							bluray_game2[gn2]=bluray_game[gn];
							bluray_game2[gn2+1]=0;
							gn2++;
						}
						
					}

					sprintf(id, "%s-[%s]", id2, bluray_game2); id[64]=0;
					
					if(curr_device==0) 
						{
						mkdir(hdd_folder, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
						sprintf(name, "%s/%s", hdd_folder, id);	

						if(cellFsStat(name, &status)== CELL_FS_SUCCEEDED)
						{
							sprintf(string1, "Destination already contains folder with the same name!\n\nPlease use FILE MANAGER [SELECT+START] to rename or remove:\n\n[%s]", name );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
							goto overwrite_cancel_bdvd;
						}
						mkdir(name, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); 
						}

					else
						{
						sprintf(name, "/dev_usb00%c/%s", 47+curr_device, ini_usb_dir);
						mkdir(name, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
						sprintf(name, "/dev_usb00%c/%s/%s", 47+curr_device, ini_usb_dir, id);
						if(cellFsStat(name, &status)== CELL_FS_SUCCEEDED)
						{
							sprintf(string1, "Destination already contains folder with the same name!\n\nPlease use FILE MANAGER [SELECT+START] to rename or remove:\n\n[%s]", name );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
							goto overwrite_cancel_bdvd;
						}
						mkdir(name, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
						}

					time_start= time(NULL);
					abort_copy=0;
					initConsole();
					file_counter=0;
					new_pad=0;
					
					if(curr_device!=0) copy_mode=1; // break files >= 4GB
						else copy_mode=0;

					copy_is_split=0;

					my_game_copy((char *) "/dev_bdvd", (char *) name);//ret=cellMsgDialogAbort();sys_timer_usleep(100000);

					int seconds= (int) (time(NULL)-time_start);
					int vflip=0;

					if(copy_is_split && !abort_copy)
						{

						if(curr_device==0) 
							sprintf(filename, "%s/%s", hdd_folder, id);	
						else
							sprintf(filename, "/dev_usb00%c/%s/_%s", 47+curr_device, ini_usb_dir, id);
						
						ret=rename(name, filename);
/*
						if(curr_device==0)   sprintf(filename, "%s\n\nSplit game copied to internal HDD (non bootable)", id);
						else
							sprintf(filename, "%s\n\nSplit game copied to external USB00%c (non bootable)", id, 47+curr_device);

						dialog_ret=0;
						ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );
						wait_dialog();
*/						
						}

					while(1)
					{

						if(abort_copy) sprintf(string1,"Aborted!  Time: %2.2i:%2.2i:%2.2i\n", seconds/3600, (seconds/60) % 60, seconds % 60);
						else
						{
							sprintf(string1,"Done! Files Copied: %i Time: %2.2i:%2.2i:%2.2i Vol: %1.2f GB\n", file_counter, seconds/3600, (seconds/60) % 60, seconds % 60, ((double) global_device_bytes)/(1024.0*1024.*1024.0));
						}

						cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

//						draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f, 0x200020ff);

						cellDbgFontPrintf( 0.07f, 0.07f, 1.2f, 0xc0c0c0c0, string1);
						
						if(vflip & 32)
						cellDbgFontPrintf( 0.5f-0.15f, 1.0f-0.07*2.0f, 1.2f, 0xc0c0c0c0, "Press [ ] to continue");
						vflip++;

						cellDbgFontDrawGcm();

						
						flip();

						pad_read();
						if (new_pad & BUTTON_SQUARE) 
							{
							new_pad=0;
							break;
							}

					}
				
			
				if(abort_copy)
					{
						if(curr_device==0)   sprintf(filename, "%s\n\nDelete incomplete folder from internal HDD?", id);
							else sprintf(filename, "%s\n\nDelete incomplete folder from USB00%c?", id, 47+curr_device);

						dialog_ret=0;
						ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
								
						wait_dialog();
						

						if(dialog_ret==1)
							{
							time_start= time(NULL);
							file_counter=0;
							abort_copy=0;
							my_game_delete((char *) name);
							rmdir((char *) name); // delete this folder

							}
						else
						{
						if(curr_device==0) 
							sprintf(filename, "%s/_%s", hdd_folder, id);	
						else
							sprintf(filename, "/dev_usb00%c/%s/_%s", 47+curr_device, ini_usb_dir, id);
						
						ret=rename(name, filename);
					
						}
					}
			

				termConsole();
				if(game_sel>max_menu_list-1) game_sel=max_menu_list-1;

				}
			}
			//forcedevices=(1);
			if(game_sel>max_menu_list-1) game_sel=max_menu_list-1;
			game_last_page=-1;
		}

overwrite_cancel_bdvd:
	join_copy=0;
	if (
		((new_pad & BUTTON_CROSS) && cover_mode==8 && 
		(xmb_icon!=6 || (xmb_icon==6 && xmb[xmb_icon].member[xmb[xmb_icon].first].type==6))) ||

		((new_pad & BUTTON_CIRCLE) && cover_mode==8 && 
		(xmb_icon==2 && xmb[xmb_icon].member[xmb[xmb_icon].first].type==7))
		) 
	{
		while(xmb_slide || xmb_slide_y){draw_whole_xmb(xmb_icon);}
		if(xmb_icon==9) 
		{
			if(xmb[9].first!=1)
				launch_web_browser(xmb[xmb_icon].member[xmb[xmb_icon].first].file_path); //web "http://www.psxstore.com/"
			else
			{
				slide_xmb_left(9);
				get_www_themes(www_theme, &max_theme);
				if(max_theme)
				{
					use_analog=1;
					float b_mX=mouseX;
					float b_mY=mouseY;
					mouseX=480.f/1920.f;
					mouseY=225.f/1080.f;
					is_any_xmb_column=xmb_icon;
					int ret_f=open_theme_menu((char*) "Download Theme", 600, www_theme, max_theme, 320, 225, 16, 1);
					is_any_xmb_column=0;
					use_analog=0;
					mouseX=b_mX;
					mouseY=b_mY;
					if(ret_f!=-1) 
					{
						char tdl[512];
						sprintf(tdl, "%s/%s.pkg", themes_web_dir, www_theme[ret_f].name);
						if(download_file(www_theme[ret_f].pkg, tdl, 4)==1 && net_used_ignore())
						{
							syscall_mount(themes_web_dir, mount_bdvd);
							dialog_ret=0;
							sprintf(string1, "Do you want to exit to XMB\xE2\x84\xA2 to install \x22%s\x22 theme from [* Install package files] menu?", www_theme[ret_f].name); 
							cellMsgDialogOpen2( type_dialog_yes_no, string1, dialog_fun1, (void*)0x0000aaaa, NULL );
							wait_dialog(); 
							if(dialog_ret!=1) {reset_mount_points(); goto cancel_theme_exit;}
							unload_modules(); sys_process_exit(1);
						};
					}
				}
cancel_theme_exit:
				slide_xmb_right();
			}
		}

		if(xmb_icon==6 && xmb[xmb_icon].member[xmb[xmb_icon].first].type==6)
		{
			if(xmb[6].first==0) goto refresh_list;
		}
		if(xmb_icon==1) //home
		{
			if(xmb[1].first==0) force_update_check=1; 
			if(xmb[1].first==1) { if(!lock_fileman) {last_cover_mode=cover_mode; cover_mode=5; new_pad=0; goto open_file_manager;}}
			if(xmb[1].first==2) goto refresh_list;
			if(xmb[1].first==3) goto switch_ntfs; 
			if(xmb[1].first==4) {screen_saver(); goto start_of_loop; }
			if(xmb[1].first==5) select_theme(); 
			if(xmb[1].first==6 && stat(helpMME, &s3)>=0 && net_used_ignore())
			{
				unload_modules();
				sys_game_process_exitspawn2((char*) helpMME, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);	
			}
			if(xmb[1].first==7 && net_used_ignore()) {
				char reload_self[128];
				sprintf(reload_self, "%s/RELOAD.SELF", app_usrdir);
				if(stat(reload_self,&s3)>=0)
				{
					unload_modules();
					sys_game_process_exitspawn2((char*) reload_self, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);	
				} 
			}
			if(xmb[1].first==8 && net_used_ignore()) {unload_modules(); sys_process_exit(1);}
//			kernel_panic(1);
/*			if(xmb[1].first==9) {
				//save_s soft reset
				//unload_modules(); 
cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP);
				char reload_self[128];
				sprintf(reload_self, "%s/EBOOT.BIN", app_usrdir);
#define NP_POOL_SIZE (128*1024)
	uint8_t np_pool[NP_POOL_SIZE];
	sceNpInit(NP_POOL_SIZE, np_pool);
	SceNpDrmKey* k_licensee = NULL;
	sceNpDrmProcessExitSpawn2(NULL, (char*)reload_self, NULL, NULL, NULL, 0, 1001, SYS_PROCESS_PRIMARY_STACK_SIZE_64K);
	sceNpTerm();
	cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
	unload_modules(); sys_process_exit(1);
			}*/
		}

		if(xmb_icon==2 && (xmb[2].member[xmb[2].first].option_size || xmb[2].first<2) ) //settings
		{
			if(xmb[2].first==1)  // clear game cache
			{
				if(delete_game_cache()!=-1)
				{
					pad_read(); new_pad=0; old_pad=0;
					dialog_ret=0; cellMsgDialogOpen2( type_dialog_back, "Game Cache Data cleared!", dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
					wait_dialog();
				}
			}

			if(xmb[2].first==0) 
			{
				char sys_info[512];

				get_free_memory();

				char line1[128];
				char line2[128];
				char line3[128];
				char line4[128];
				char line5[64];

				cellFsGetFreeSize((char*)"/dev_hdd0", &blockSize, &freeSize);
				freeSpace = ( ((uint64_t)blockSize * freeSize));
				sprintf(line5, "Free HDD space: %.2f GB", (double) (freeSpace / 1073741824.00f));

				strncpy(line4, current_version, 8); line4[8]=0;
				sprintf(line1, "Free Memory: %.0f KB", (double) (meminfo.avail/1024.0f));//, (double) ((meminfo.avail+meminfo.total)/1024.0f)
				if(payload==-1) sprintf(line2, "PS3\xE2\x84\xA2 System: Firmware %.2f", c_firmware);
				if(payload== 0 && payloadT[0]!=0x44) sprintf(line2, "PS3\xE2\x84\xA2 System: Firmware %.2f [SC-36 | PSGroove]", c_firmware);
				if(payload== 0 && payloadT[0]==0x44) sprintf(line2, "PS3\xE2\x84\xA2 System: Firmware %.2f [SC-36 | Standard]", c_firmware);
				if(payload== 1) sprintf(line2, "PS3\xE2\x84\xA2 System: Firmware %.2f [SC-8 | Hermes]", c_firmware);
				if(payload== 2) sprintf(line2, "PS3\xE2\x84\xA2 System: Firmware %.2f [SC-35 | PL3]", c_firmware);

				net_avail=cellNetCtlGetInfo(16, &net_info);
				if(net_avail<0) 
				{
					sprintf(line3, "%s", "IP Address: [Not Available]");
				}
				else
					sprintf(line3, "IP Address: %s", net_info.ip_address);
				is_any_xmb_column=xmb_icon;
				parse_ini(options_ini,1);
				sprintf(sys_info, "multiMAN Version: %s\n\n%s\n%s\n%s\n%s", line4, line2, line3, line1, line5);
				dialog_ret=0; cellMsgDialogOpen2( type_dialog_back, sys_info, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
				is_any_xmb_column=0; 
				pad_read(); new_pad=0; old_pad=0;
				goto xmb_cancel_option;
			}

			if(xmb[2].first>1) xmb[2].member[xmb[2].first].data=-1;

			if(!strcmp(xmb[2].member[xmb[2].first].optionini, "parental_level") || !strcmp(xmb[2].member[xmb[2].first].optionini, "parental_pass") || !strcmp(xmb[2].member[xmb[2].first].optionini, "disable_options") || !strcmp(xmb[2].member[xmb[2].first].optionini, "lock_fileman"))
			{
				if(parental_pin_entered && (!strcmp(xmb[2].member[xmb[2].first].optionini, "disable_options") || !strcmp(xmb[2].member[xmb[2].first].optionini, "parental_level") || !strcmp(xmb[2].member[xmb[2].first].optionini, "lock_fileman") )) goto xmb_pin_ok;

				{
					sprintf(string1, "%s", "Enter access PIN code:");

						OutputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
						OutputInfo.numCharsResultString = 128;
						OutputInfo.pResultString = Result_Text_Buffer;
						open_osk(3, (char*) string1);
						is_any_xmb_column=xmb_icon;
						while(1){ 
							//draw_xmb_bare(2, 1, 0, 0);
							draw_whole_xmb(xmb_icon);
							if(osk_dialog==1 || osk_dialog==-1) break;
							}
						cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
						flip(); cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
						flipc(30);
						is_any_xmb_column=0;
						osk_open=0;
						if(osk_dialog!=0)
						{
							char pin_result[32];
							wchar_t *pin_result2;
							pin_result2 = (wchar_t*)OutputInfo.pResultString;
							wcstombs(pin_result, pin_result2, 4);
							if(strlen(pin_result)==4) {
								if(strcmp(pin_result, parental_pass)==0) {
									parental_pin_entered=1;
									goto xmb_pin_ok;
								}
							}
						}
						dialog_ret=0;
						parental_pin_entered=0;
						cellMsgDialogOpen2( type_dialog_ok, "Entered PIN code is incorrect!", dialog_fun2, (void*)0x0000aaab, NULL );					
						wait_dialog();
						goto xmb_cancel_option;
				}

				goto xmb_cancel_option;
			}

xmb_pin_ok:

			if(!strcmp(xmb[2].member[xmb[2].first].optionini, "parental_pass"))
				{
					sprintf(string1, "%s", "Enter NEW access PIN code:");

						OutputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
						OutputInfo.numCharsResultString = 128;
						OutputInfo.pResultString = Result_Text_Buffer;
						open_osk(3, (char*) string1);

						while(1){ 
							draw_xmb_bare(2, 1, 0, 0);
							if(osk_dialog==1 || osk_dialog==-1) break;
							}
						cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
						flip(); cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
						flipc(60);
						osk_open=0;
						parental_pin_entered=0;
						if(osk_dialog!=0)
						{
							char pin_result[32];
							wchar_t *pin_result2;
							pin_result2 = (wchar_t*)OutputInfo.pResultString;
							wcstombs(pin_result, pin_result2, 4);
							if(strlen(pin_result)==4) {
									sprintf(parental_pass, "%s", pin_result);
									goto xmb_pin_ok2;
							}
						}
						dialog_ret=0;
						cellMsgDialogOpen2( type_dialog_ok, "Entered PIN code is not accepted!\n\nPlease use four character alphanumeric PIN!", dialog_fun2, (void*)0x0000aaab, NULL );					
						wait_dialog();
						goto xmb_cancel_option;
				}


xmb_pin_ok2:

			if(new_pad & BUTTON_CIRCLE)
			{
				if(xmb[2].member[xmb[2].first].option_selected<1) xmb[2].member[xmb[2].first].option_selected=xmb[2].member[xmb[2].first].option_size;
				xmb[2].member[xmb[2].first].option_selected--;
			}
			else
			{
				xmb[2].member[xmb[2].first].option_selected++;
				if(xmb[2].member[xmb[2].first].option_selected>=xmb[2].member[xmb[2].first].option_size) xmb[2].member[xmb[2].first].option_selected=0;
			}

			parse_settings();
			if(!strcmp(xmb[2].member[xmb[2].first].optionini, "confirm_with_x")) {set_xo();xmb_legend_drawn=0;}
			if(!strcmp(xmb[2].member[xmb[2].first].optionini, "display_mode") || !strcmp(xmb[2].member[xmb[2].first].optionini, "hide_bd")) forcedevices=(1<<11);//0x0800;
			if(!strcmp(xmb[2].member[xmb[2].first].optionini, "bd_emulator") && xmb[2].member[xmb[2].first].option_selected==1 && !bdemu2_present) 
			{
				if(c_firmware==3.55f)
					xmb[2].member[xmb[2].first].option_selected=2;
				else
					xmb[2].member[xmb[2].first].option_selected=0;

				dialog_ret=0;
				cellMsgDialogOpen2( type_dialog_ok, "Selected option is not available.\n\nERROR: Incorrect BDEMU version!", dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();

			}

			if(!strcmp(xmb[2].member[xmb[2].first].optionini, "lock_fileman"))
			{
				if(lock_fileman) sprintf(xmb[1].member[1].name, "%s", "File Manager (Disabled)");
				else sprintf(xmb[1].member[1].name, "%s", "File Manager");
				for(int n=0; n<MAX_XMB_TEXTS; n++)
				{
					xmb_txt_buf[n].used=0;
				}
				xmb_txt_buf_max=0;
				for(int n=0; n<xmb[1].size; n++) xmb[1].member[n].data=-1;
				for(int n=0; n<xmb[2].size; n++) xmb[2].member[n].data=-1;

			}

			if(!strcmp(xmb[2].member[xmb[2].first].optionini, "theme_sound"))
			{

				if((!(current_mp3!=0 && max_mp3!=0)) && !theme_sound)
					main_mp3((char*)"SILENCE.BIN");
				if((!(current_mp3!=0 && max_mp3!=0)) && theme_sound)
						sprintf(filename, "%s/SOUND.BIN", app_usrdir);
						if(stat(filename, &s3)>=0)
							main_mp3((char*)filename); 
			}

			if(!strcmp(xmb[2].member[xmb[2].first].optionini, "user_font")) redraw_column_texts(xmb_icon);


xmb_cancel_option:
			//redraw_column_texts(xmb_icon);
			dialog_ret=0;

		}


		if(xmb_icon==4 && xmb[xmb_icon].member[xmb[xmb_icon].first].type==4) //music
		{
			char aufile[512];
			sprintf(aufile, "%s", xmb[xmb_icon].member[xmb[xmb_icon].first].file_path);
			if(stat(aufile, &s3)>=0 && (strstr(aufile, ".mp3")!=NULL || strstr(aufile, ".MP3")!=NULL)) 
			{
				int ci2;
				max_mp3=1;
				current_mp3=1;
				sprintf(mp3_playlist[max_mp3].path, "%s", aufile);
				
				//add the rest of the files as a playlist
				for(ci2=xmb[xmb_icon].first+1; ci2<xmb[xmb_icon].size; ci2++)
				{
					sprintf(aufile, "%s", xmb[xmb_icon].member[ci2].file_path);
					if(strstr(aufile, ".mp3")!=NULL || strstr(aufile, ".MP3")!=NULL)
					{
						if(max_mp3>=MAX_MP3) break;
						max_mp3++;
						sprintf(mp3_playlist[max_mp3].path, "%s", aufile);

					}

				}

				for(ci2=1; ci2<(xmb[xmb_icon].first-1); ci2++)
				{
					sprintf(aufile, "%s", xmb[xmb_icon].member[ci2].file_path);
					if(strstr(aufile, ".mp3")!=NULL || strstr(aufile, ".MP3")!=NULL)
					{
						if(max_mp3>=MAX_MP3) break;
						max_mp3++;
						sprintf(mp3_playlist[max_mp3].path, "%s", aufile);
					}

				}
				main_mp3((char*) mp3_playlist[1].path);

			}
			else if(stat(aufile, &s3)>=0) goto retry_showtime_xmb;
		}

		if(xmb_icon==5 && ( (xmb[xmb_icon].member[xmb[xmb_icon].first].type==3) || xmb[xmb_icon].first<2) && net_used_ignore()) //video for showtime
		{

			sprintf(filename, "%s/XMB Video", app_usrdir);
			if(stat(filename, &s3)<0) mkdir(filename, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

			if(xmb[xmb_icon].first==0)
			{
				char linkfile[512];
				max_dir_l=0;

				sprintf(filename, "%s/XMB Video", app_usrdir);
				if(stat(filename, &s3)>=0) del_temp(filename);

				ps3_home_scan_bare2((char*)"/dev_hdd0/video", pane_l, &max_dir_l); 
				ps3_home_scan_bare2((char*)"/dev_hdd0/VIDEO", pane_l, &max_dir_l); 

				for(int ret_f=0; ret_f<max_dir_l; ret_f++)
					if( is_video(pane_l[ret_f].name)  )
				{

						sprintf(linkfile, "%s/%s", pane_l[ret_f].path, pane_l[ret_f].name);
						sprintf(filename, "%s/XMB Video/%s", app_usrdir, pane_l[ret_f].name);
						link(linkfile, filename);
				}

			}



retry_showtime_xmb:
			sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
			if(stat(filename, &s3)>=0)
			{

			if(xmb[xmb_icon].first<2 && net_used_ignore())
				{
					unload_modules();
					sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
					sys_game_process_exitspawn2(filename, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
				}

				FILE *flist; 
				sprintf(string1, "%s/TEMP/SHOWTIME.TXT", app_usrdir);
				remove(string1);
				flist = fopen(string1, "w");
				sprintf(filename, "file://%s", xmb[xmb_icon].member[xmb[xmb_icon].first].file_path);fputs (filename,  flist ); 
				fclose(flist); 
				if(net_used_ignore())
				{
					unload_modules();
					sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
					sys_game_process_exitspawn2(filename, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
				}
			}
			else
			{
				cellMsgDialogAbort();
				dialog_ret=0;
				sprintf(filename, "Showtime for multiMAN application is missing!\n\nDo you want to download it now?");
				cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );
				wait_dialog();
				if(dialog_ret==1)
				{
					sprintf(filename, "%s/SHOWTIME.SELF", app_usrdir);
					sprintf(string1, "%s/SHOWTIME.SELF", url_base);
					download_file(string1, filename, 1);
					goto retry_showtime_xmb;
				}
			}
		}

		if(xmb_icon==8) 
		{ // emulators
			if(xmb[xmb_icon].first==0)
			{
				xmb[xmb_icon].init=0;
				xmb[xmb_icon].size=0;
				free_all_buffers();
				sprintf(string1, "%s/XMBS.008", app_usrdir);
				remove(string1);
				add_emulator_column();
			}
			else
			{
				if(xmb[xmb_icon].member[xmb[xmb_icon].first].type==8 ) launch_snes_emu(xmb[xmb_icon].member[xmb[xmb_icon].first].file_path);
				else if(xmb[xmb_icon].member[xmb[xmb_icon].first].type==9) launch_fceu_emu(xmb[xmb_icon].member[xmb[xmb_icon].first].file_path);
				else if(xmb[xmb_icon].member[xmb[xmb_icon].first].type==10) launch_vba_emu(xmb[xmb_icon].member[xmb[xmb_icon].first].file_path);
				else if(xmb[xmb_icon].member[xmb[xmb_icon].first].type==11) launch_genp_emu(xmb[xmb_icon].member[xmb[xmb_icon].first].file_path);
			}
		}

		if(xmb_icon==3) 
		{ //photo{
			while(is_decoding_jpg){	draw_whole_xmb(xmb_icon);}//just wait for threaded decoding to finish
			int current_image=xmb[xmb_icon].first;
			char image_file[512];
			long slide_time=0;
			int slide_show=0;
			int show_info=0;
			sprintf(image_file, "%s", xmb[xmb_icon].member[current_image].file_path);

			if(strstr(image_file, ".jpg")!=NULL || strstr(image_file, ".JPG")!=NULL || strstr(image_file, ".jpeg")!=NULL || strstr(image_file, ".JPEG")!=NULL || strstr(image_file, ".png")!=NULL || strstr(image_file, ".PNG")!=NULL)
				{
			int to_break=0, slide_dir=0;
			float pic_zoom=1.0f;
			int	pic_reload=1, pic_posY=0, pic_posX=0, pic_X=0, pic_Y=0;
			char pic_info[512];
			mouseYDR=mouseXDR=mouseYDL=mouseXDL=0.0000f;
			while(1)
				{ // Picture Viewer Mode
				use_analog=1;
				//cellSysutilCheckCallback();
				sprintf(image_file, "%s", xmb[xmb_icon].member[current_image].file_path);
				if(strstr(image_file, ".jpg")!=NULL || strstr(image_file, ".JPG")!=NULL || strstr(image_file, ".jpeg")!=NULL || strstr(image_file, ".JPEG")!=NULL)
				{
						//cellDbgFontDrawGcm(); cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);

						if(pic_reload!=0){
							cellDbgFontDrawGcm();
							pic_zoom=-1.0f;
							/*if(strstr(xmb[xmb_icon].member[current_image].file_path,"/net_host")!=NULL) //network copy
							{

								char cpath[1024], cpath2[1024];
								int chost=0; int pl=strlen(xmb[xmb_icon].member[current_image].file_path);
								chost=xmb[xmb_icon].member[current_image].file_path[9]-0x30;
								for(int n=11;n<pl;n++)
								{cpath[n-11]=xmb[xmb_icon].member[current_image].file_path[n]; cpath[n-10]=0;}
								sprintf(cpath2, "/%s", cpath); //host_list[chost].root,, 
								sprintf(image_file, "%s/TEMP/net_view.bin", app_usrdir);
								network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) image_file, 0);
							} */

							if(strstr(xmb[xmb_icon].member[current_image].file_path,"/pvd_usb")!=NULL) //ntfs
							{
								sprintf(image_file, "%s/TEMP/net_view.bin", app_usrdir);
								file_copy(xmb[xmb_icon].member[current_image].file_path, image_file, 0);
							}

							load_jpg_texture(text_bmp, image_file, 1920);
							slide_time=0;
						}
						png_w2=png_w; png_h2=png_h;
						if(pic_zoom==-1.0f){
							pic_zoom=1.0f;
							//if(png_w==1280 && png_h==720) pic_zoom=1920.0f/1280.0f;
							//if(png_w==640 && png_h==360) pic_zoom=3.0f;
							if(png_h!=0 && png_h>=png_w && (float)png_h/(float)png_w>=1.77f)
								pic_zoom=float (1080.0f / png_h);
							if(png_h!=0 && png_h>=png_w && (float)png_h/(float)png_w<1.77f)
								pic_zoom=float (1920.0f / png_h);
							else if(png_w!=0 && png_h!=0 && png_w>png_h && (float)png_w/(float)png_h>=1.77f)
								pic_zoom=float (1920.0f / png_w);
							else if(png_w!=0 && png_h!=0 && png_w>png_h && (float)png_w/(float)png_h<1.77f)
								pic_zoom=float (1080.0f / png_h);
						}
						if(pic_zoom>4.f) pic_zoom=4.f;
						png_h2=(int) (png_h2*pic_zoom);
						png_w2=(int) (png_w2*pic_zoom);
						if(pic_reload!=0)
						{
							if(slide_dir==0)
								for(int slide_in=1920; slide_in>=0; slide_in-=128)
								{	flip();
									if(key_repeat && abs(slide_in)>640) break;
									cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
									set_texture( text_bmp, 1920, 1080); 
									display_img((int)((1920-png_w2)/2)+pic_posX+slide_in, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
								}
							else
								for(int slide_in=-1920; slide_in<=0; slide_in+=128)
								{	flip();
									if(key_repeat && abs(slide_in)>640) break;
									cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
									set_texture( text_bmp, 1920, 1080); 
									display_img((int)((1920-png_w2)/2)+pic_posX+slide_in, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
								}
						}
						else
							{
								cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
								set_texture( text_bmp, 1920, 1080); 
								display_img((int)((1920-png_w2)/2)+pic_posX, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
							}
/*						if(show_info==1){

							sprintf(pic_info,"Name: %s", xmb[xmb_icon].member[current_image].name); pic_info[95]=0;
							draw_text_stroke( 0.04f+new_offset, 0.867f, 0.7f ,0xc0a0a0a0, pic_info);
							//timeinfo = localtime ( &xmb[xmb_icon].member[current_image].time );
							sprintf(pic_info,"Info: JPEG %ix%i (Zoom: %3.0f)\nDate: %s", png_w, png_h, pic_zoom*100.0f, xmb[xmb_icon].member[current_image].subname);
							draw_text_stroke( 0.04f+new_offset, 0.89f, 0.7f ,0xc0a0a0a0, pic_info);

							cellDbgFontDrawGcm();
						}
*/

				}

				if(strstr(image_file, ".png")!=NULL || strstr(image_file, ".PNG")!=NULL)
				{
						cellDbgFontDrawGcm();
						if(pic_reload!=0){
							/*if(strstr(xmb[xmb_icon].member[current_image].file_path,"/net_host")!=NULL) //network copy
							{

								char cpath[1024], cpath2[1024];
								int chost=0; int pl=strlen(xmb[xmb_icon].member[current_image].file_path);
								chost=xmb[xmb_icon].member[current_image].file_path[9]-0x30;
								for(int n=11;n<pl;n++)
								{cpath[n-11]=xmb[xmb_icon].member[current_image].file_path[n]; cpath[n-10]=0;}
								sprintf(cpath2, "/%s", cpath); //host_list[chost].root,
								sprintf(image_file, "%s/TEMP/net_view.bin", app_usrdir);
								network_com((char*)"GET", (char*)host_list[chost].host, host_list[chost].port, (char*) cpath2, (char*) image_file, 0);
							}*/

							if(strstr(xmb[xmb_icon].member[current_image].file_path,"/pvd_usb")!=NULL) //ntfs
							{
								sprintf(image_file, "%s/TEMP/net_view.bin", app_usrdir);
								file_copy(xmb[xmb_icon].member[current_image].file_path, image_file, 0);
							}

							load_png_texture(text_bmp, image_file, 1920);
							slide_time=0;
							//if(png_w>1920 || png_h>1080) goto cancel_exit;
						}

						png_w2=png_w; png_h2=png_h;
						if(pic_zoom==-1.0f){
							pic_zoom=1.0f;
							//if(png_w==1280 && png_h==720) pic_zoom=1920.0f/1280.0f;
							//if(png_w==640 && png_h==360) pic_zoom=3.0f;
							if(png_h!=0 && png_h>=png_w && (float)png_h/(float)png_w>=1.77f)
								pic_zoom=float (1080.0f / png_h);
							if(png_h!=0 && png_h>=png_w && (float)png_h/(float)png_w<1.77f)
								pic_zoom=float (1920.0f / png_h);
							else if(png_w!=0 && png_h!=0 && png_w>png_h && (float)png_w/(float)png_h>=1.77f)
								pic_zoom=float (1920.0f / png_w);
							else if(png_w!=0 && png_h!=0 && png_w>png_h && (float)png_w/(float)png_h<1.77f)
								pic_zoom=float (1080.0f / png_h);
						}
						if(pic_zoom>4.f)pic_zoom=4;
						png_h2=(int) (png_h2*pic_zoom);
						png_w2=(int) (png_w2*pic_zoom);
						pic_X=(int)((1920-png_w2)/2)+pic_posX;
						pic_Y=(int)((1080-png_h2)/2)+pic_posY;

						if(pic_reload!=0)
						{
							if(slide_dir==0)
								for(int slide_in=1920; slide_in>=0; slide_in-=128)
								{	flip();
									if(key_repeat && abs(slide_in)>640) break;
									cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
									set_texture( text_bmp, 1920, 1080); 
									display_img((int)((1920-png_w2)/2)+pic_posX+slide_in, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
								}
							else
								for(int slide_in=-1920; slide_in<=0; slide_in+=128)
								{	flip();
									if(key_repeat && abs(slide_in)>640) break;
									cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
									set_texture( text_bmp, 1920, 1080); 
									display_img((int)((1920-png_w2)/2)+pic_posX+slide_in, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
								}
						}
						else
							{
								cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
								set_texture( text_bmp, 1920, 1080); 
								display_img(pic_X, pic_Y, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
							}

				}

					//flip();
					int ci=current_image;
					to_break=0;
					char ss_status[8];

					while(1){
						//cellSysutilCheckCallback();
						pad_read();
						cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
						set_texture( text_bmp, 1920, 1080); 
						if(strstr(image_file, ".png")!=NULL || strstr(image_file, ".PNG")!=NULL)
							display_img(pic_X, pic_Y, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
						else
							display_img((int)((1920-png_w2)/2)+pic_posX, (int)((1080-png_h2)/2)+pic_posY, png_w2, png_h2, png_w, png_h, 0.0f, 1920, 1080);
						if(show_info==1){
							if(slide_show) sprintf(ss_status, "%s", "Stop"); else sprintf(ss_status, "%s", "Start");
							sprintf(pic_info,"   Name: %s", xmb[xmb_icon].member[current_image].name); pic_info[95]=0;
							draw_text_stroke( 0.04f+new_offset, 0.867f, 0.7f ,0xc0a0a0a0, pic_info);
							//timeinfo = localtime ( &xmb[xmb_icon].member[current_image].time );
							if(strstr(image_file, ".png")!=NULL || strstr(image_file, ".PNG")!=NULL)
								sprintf(pic_info,"   Info: PNG %ix%i (Zoom: %3.0f)\n   Date: %s\n[START]: %s slideshow", png_w, png_h, pic_zoom*100.0f, xmb[xmb_icon].member[current_image].subname, ss_status);
							else
								sprintf(pic_info,"   Info: JPEG %ix%i (Zoom: %3.0f)\n   Date: %s\n[START]: %s slideshow", png_w, png_h, pic_zoom*100.0f, xmb[xmb_icon].member[current_image].subname, ss_status);

							draw_text_stroke( 0.04f+new_offset, 0.89f, 0.7f ,0xc0a0a0a0, pic_info);

							cellDbgFontDrawGcm();
						}
						flip();


						if ( new_pad & BUTTON_SELECT ) {show_info=1-show_info; pic_reload=0; break;}//new_pad=0; old_pad=0; 

						if ( new_pad & BUTTON_START ) {
							slide_time=0; //new_pad=0; old_pad=0;
							slide_show=1-slide_show; slide_dir=0;
						}

						if(slide_show==1) slide_time++;

						if ( ( new_pad & BUTTON_TRIANGLE ) || ( new_pad & BUTTON_CIRCLE ) ) {to_break=1;break;}//new_pad=0; old_pad=0;

						if ( ( new_pad & BUTTON_RIGHT ) || ( new_pad & BUTTON_R1 ) || ( new_pad & BUTTON_CROSS ) || (slide_show==1 && slide_time>600) )
						{
							//find next image in the list
							for(ci=current_image+1; ci<xmb[xmb_icon].size; ci++)
							{
								sprintf(image_file, "%s", xmb[xmb_icon].member[ci].file_path);
								if(strstr(image_file, ".jpg")!=NULL || strstr(image_file, ".JPG")!=NULL || strstr(image_file, ".jpeg")!=NULL || strstr(image_file, ".JPEG")!=NULL || strstr(image_file, ".png")!=NULL || strstr(image_file, ".PNG")!=NULL)
								{
									current_image=ci; 
									xmb[xmb_icon].first=ci;
									pic_zoom=1.0f;
									pic_reload=1;
									pic_posX=pic_posY=0;
									slide_time=0;
									slide_dir=0;
									break;
								}

							}
							
							if(current_image>=xmb[xmb_icon].size || ci>=xmb[xmb_icon].size) current_image=0;//to_break=1; // || current_image==xmb[xmb_icon].first
							break;
							
						}

						if ( ( new_pad & BUTTON_LEFT ) || ( new_pad & BUTTON_L1 ) ) 
						{
							//find previous image in the list
							if(current_image==0) current_image=xmb[xmb_icon].size;
							int one_time2=1;
check_from_start2:
							for(ci=current_image-1; ci>=0; ci--)
							{
								sprintf(image_file, "%s", xmb[xmb_icon].member[ci].file_path);
								if(strstr(image_file, ".jpg")!=NULL || strstr(image_file, ".JPG")!=NULL || strstr(image_file, ".jpeg")!=NULL || strstr(image_file, ".JPEG")!=NULL || strstr(image_file, ".png")!=NULL || strstr(image_file, ".PNG")!=NULL)
								{
									current_image=ci; 
									xmb[xmb_icon].first=ci;
									pic_zoom=1.0f;
									pic_reload=1;
									pic_posX=pic_posY=0;
									slide_show=0; slide_dir=1;
									break;
								}

							}
							
							if((current_image<0 || ci<0) && one_time2) {one_time2=0; current_image=xmb[xmb_icon].size; goto check_from_start2;}// to_break=1; // || current_image==e
							break;
							
						}

						if (( new_pad & BUTTON_L3 ) || ( new_pad & BUTTON_DOWN )) 
						{
							if(png_w!=0 && pic_zoom==1.0f)
								pic_zoom=float (1920.0f / png_w);
							else
								pic_zoom=1.0f;
							pic_reload=0;
							pic_posX=pic_posY=0;
							new_pad=0; old_pad=0;
							break;
						}

						if (( new_pad & BUTTON_R3 ) || ( new_pad & BUTTON_UP )) 
						{
							if(png_h!=0 && pic_zoom==1.0f)
								pic_zoom=float (1080.0f / png_h);
							else
								pic_zoom=1.0f;
							pic_reload=0;
							pic_posX=pic_posY=0;
							new_pad=0; old_pad=0;
							break;
						}

						if (mouseXDL!=0.0f && png_w2>1920) 
						{
							pic_posX-=(int) (mouseXDL*1920.0f);
							pic_reload=0;

							if( pic_posX<(int)((1920-png_w2)/2) ) pic_posX=(int)((1920-png_w2)/2);
							if( ((int)((1920-png_w2)/2)+pic_posX)>0 ) pic_posX=0-(int)((1920-png_w2)/2);
							break;
						}

						if (mouseYDL!=0.0f && png_h2>1080) 
						{
							pic_posY-=(int) (mouseYDL*1080.0f);

							if( pic_posY<(int)((1080-png_h2)/2) ) pic_posY=(int)((1080-png_h2)/2);
							if( ((int)((1080-png_h2)/2)+pic_posY)>0 ) pic_posY=0-(int)((1080-png_h2)/2);

							pic_reload=0;
							break;
						}

						if (( new_pad & BUTTON_L2 ) || mouseXDR> 0.003f || mouseYDR> 0.003f) 
						{
							if ( new_pad & BUTTON_L2 )
								pic_zoom-=0.045f;
							else
								pic_zoom-=0.010f;
							if(pic_zoom<1.0f) pic_zoom=1.000f;
							pic_reload=0;
							png_h2=(int) (png_h2*pic_zoom);
							png_w2=(int) (png_w2*pic_zoom);
							if( pic_posX<(int)((1920-png_w2)/2) ) pic_posX=(int)((1920-png_w2)/2);
							if( ((int)((1920-png_w2)/2)+pic_posX)>0 ) pic_posX=0;
							if( pic_posY<(int)((1080-png_h2)/2) ) pic_posY=(int)((1080-png_h2)/2);
							if( ((int)((1080-png_h2)/2)+pic_posY)>0 ) pic_posY=0;
							break;
						}

						if (( new_pad & BUTTON_R2 ) || mouseXDR< -0.003f || mouseYDR< -0.003f) 
						{
							if (new_pad & BUTTON_R2) 
								pic_zoom+=0.045f;
							else
								pic_zoom+=0.010f;
							pic_reload=0;
							png_h2=(int) (png_h2*pic_zoom);
							png_w2=(int) (png_w2*pic_zoom);
							if( pic_posX<(int)((1920-png_w2)/2) ) pic_posX=(int)((1920-png_w2)/2);
							if( ((int)((1920-png_w2)/2)+pic_posX)>0 ) pic_posX=0;
							if( pic_posY<(int)((1080-png_h2)/2) ) pic_posY=(int)((1080-png_h2)/2);
							if( ((int)((1080-png_h2)/2)+pic_posY)>0 ) pic_posY=0;
							break;
						}

					}
					new_pad=0;

					if(to_break==1) break; 

				} //picture viewer
				use_analog=0;

				}
				load_texture(text_bmp, xmbbg, 1920);
				}


		




	}

	if (new_pad & BUTTON_CROSS && game_sel>=0 && (((mode_list==0) && max_menu_list>0)) && strstr(menu_list[game_sel].path,"/pvd_usb")==NULL && (cover_mode!=8 || (cover_mode==8 && ( (xmb_icon==6 && xmb[xmb_icon].member[xmb[xmb_icon].first].type==1 && xmb[xmb_icon].size>1) || (xmb_icon==7  && xmb[xmb_icon].size) || (xmb_icon==5 && xmb[xmb_icon].member[xmb[xmb_icon].first].type==2)))) ) {

start_title:
		join_copy=0;
		struct stat s;
		c_opacity_delta=16;	dimc=0; dim=1;
/*
		if(mode_list==1)	
			{
//			int prio = 1001;
//	    	uint64_t flags = SYS_PROCESS_PRIMARY_STACK_SIZE_64K;
			sprintf(filename, "/dev_hdd0/game/%s/USRDIR/homebrew/PS3_GAME", app_path);
			time_start= time(NULL);
	
			abort_copy=0;
			initConsole();
			file_counter=0;
			new_pad=0;
			copy_mode=0;
			copy_is_split=0;
			my_game_copy((char *) menu_homebrew_list[game_sel].path, (char *) filename);
			flip();

			sprintf(filename, "/dev_hdd0/game/%s/USRDIR/homebrew/PARAM_HOMEBREW.SFO", app_path);
			sprintf(string1,  "/dev_hdd0/game/%s/USRDIR/homebrew/PS3_GAME/PARAM.SFO", app_path);
//			file_copy((char *) filename, (char *) string1);

			sprintf(filename, "/dev_hdd0/game/%s/USRDIR/homebrew", app_path);
			syscall_mount( (char *) filename, mount_bdvd, (char *)mount_point);
			ret = unload_modules();
			sprintf(filename, "/dev_hdd0/game/%s/USRDIR/homebrew/PS3_GAME/USRDIR/EBOOT.BIN", app_path);
//			sys_game_process_exitspawn(filename, NULL, NULL, 0, 0, prio, flags);
			sys_game_process_exitspawn2(filename, NULL, NULL, NULL, 0, 3071, SYS_PROCESS_PRIMARY_STACK_SIZE_512K);
			exit(0);
			}
*/

		if(parental_level<menu_list[game_sel].plevel && parental_level>0)
		{
			sprintf(string1, "Game parental level: %i - Enter access PIN code:", menu_list[game_sel].plevel );

				OutputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
				OutputInfo.numCharsResultString = 128;
				OutputInfo.pResultString = Result_Text_Buffer;
				open_osk(3, (char*) string1);

				while(1){ 
					sprintf(string1, "::: %s :::\n\n\nSelected game is restricted with parental level %i.\n\nPlease enter four alphanumeric parental password code:", menu_list[game_sel].title, menu_list[game_sel].plevel);
					cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
					draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.9f, 0xd0000080);
					cellDbgFontPrintf( 0.10f, 0.10f, 1.0f, 0xffffffff, string1);
					setRenderColor();
					cellDbgFontDrawGcm();
					flip(); 
					//cellSysutilCheckCallback();
					if(osk_dialog==1 || osk_dialog==-1) break;
					}
				cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
				flip(); cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
				flipc(60);
				osk_open=0;
				if(osk_dialog!=0)
				{
					char pin_result[32];
					wchar_t *pin_result2;
					pin_result2 = (wchar_t*)OutputInfo.pResultString;
					wcstombs(pin_result, pin_result2, 4);
					if(strlen(pin_result)==4) {
						if(strcmp(pin_result, parental_pass)==0) {
							goto pass_ok;
						}
					}
				}
				dialog_ret=0;
				cellMsgDialogOpen2( type_dialog_ok, "Entered PIN code is incorrect!", dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();
				goto skip_1;

		}

pass_ok:
		direct_launch_forced=0;
		if( (old_pad & BUTTON_SELECT) || (menu_list[game_sel].user & IS_DBOOT)) direct_launch_forced=1;
		char fileboot[1024]; memset(fileboot, 0, 1023);

		if( (menu_list[game_sel].flags & 2048) && net_used_ignore())
		{
			flip();
			if(direct_launch_forced && !(menu_list[game_sel].user & IS_DBOOT))
			{

				sprintf(filename, "%s/PS3_GAME/USRDIR/EBOOT.BIN", menu_list[game_sel].path);
//				syscall_mount2((char *)"/dev_usb000", (char *)menu_list[game_sel].path);
				if(stat(filename, &s)>=0)
				{

					sprintf(fileboot, "%s/PS3_GAME/USRDIR/EBOOT.BIN", menu_list[game_sel].path);

					if( ( (payload==0 && sc36_path_patch==0) || (menu_list[game_sel].user & IS_DISC) ) && stat((char*)"/dev_bdvd", &s3)<0)
					{
						dialog_ret=0;
						cellMsgDialogOpen2( type_dialog_yes_back, "Please insert an original PLAYSTATION\xC2\xAE\x33 game disc before proceeding!", dialog_fun1, (void*)0x0000aaaa, NULL );
						wait_dialog();
						if(dialog_ret==3 || stat((char*)"/dev_bdvd", &s3)<0) goto cancel_exit_2;
					}
					else
						if(payload==0 && sc36_path_patch==1 && stat((char*)"/dev_bdvd", &s3)<0)
						{
							//dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, "Start your game from [* /app_home] menu.\n\nShould you run into problems - insert an original Playstation(R)3 game disc next time!", dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
							poke_sc36_path( (char *) "/app_home" );
						}


					dialog_ret=0;
					/*if(direct_launch==1) 
					{	sprintf(string1, "Do you want to start the game without exiting to XMB?\n\nWarning: Some games do not support such launch mode!"); 
						ret = cellMsgDialogOpen2( type_dialog_yes_no, string1, dialog_fun1, (void*)0x0000aaaa, NULL );
						wait_dialog(); 
					}
					if(dialog_ret==3) {goto cancel_exit_2;} */

					if(menu_list[game_sel].user & IS_DBOOT)  
					{
						write_last_play( (char *)fileboot, (char *)menu_list[game_sel].path, (char *)menu_list[game_sel].title, (char *)menu_list[game_sel].title_id, 1);
						unload_modules();
						//if(payload==0)	
						//sys_game_process_exitspawn2((char *) fileboot, NULL, NULL, NULL, 0, 1001, SYS_PROCESS_PRIMARY_STACK_SIZE_128K);
						//else 
						sprintf(string1, "%s", menu_list[game_sel].path);
						sprintf(fileboot, "%s/PS3_GAME/USRDIR/EBOOT.BIN", menu_list[game_sel].path);
						syscall_mount( string1, mount_bdvd);
						sys_game_process_exitspawn2((char *) fileboot, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_128K);
//						sys_game_process_exitspawn2((char *) "/app_home/PS3_GAME/USRDIR/EBOOT.BIN", NULL, NULL, NULL, 0, 1001, SYS_PROCESS_PRIMARY_STACK_SIZE_128K);
						
					}
					else
					{
						write_last_play( (char *)fileboot, (char *)menu_list[game_sel].path, (char *)menu_list[game_sel].title, (char *)menu_list[game_sel].title_id, 0);
 						unload_modules();
						sprintf(string1, "%s", menu_list[game_sel].path);
						syscall_mount( string1, mount_bdvd);
 						sys_process_exit(1); break;
					}
				}

			}
			else
			{
				reset_mount_points();
				ret = unload_modules();
				sys_process_exit(1);
			}
		}

		if( (strstr(menu_list[game_sel].content,"AVCHD")!=NULL || strstr(menu_list[game_sel].content,"BDMV")!=NULL) && net_used_ignore()) // Rename/activate USB HDD AVCHD folder
		{

		if(strstr(menu_list[game_sel].content,"BDMV")!=NULL && strstr(menu_list[game_sel].path,"/dev_hdd0")!=NULL)
		{
			sprintf(filename, "::: %s :::\n\n%s: %s\n\nDo you want to convert the Blu-ray\xE2\x84\xA2 disc structure to AVCHD\xE2\x84\xA2 format?\n\nNote: The action may improve playback compatibility!", menu_list[game_sel].title, menu_list[game_sel].entry, menu_list[game_sel].details);
			dialog_ret=0;
			ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaab, NULL );
			wait_dialog();
			if(dialog_ret==1)  
			{
				dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Converting Blu-ray\xE2\x84\xA2 structure to AVCHD\xE2\x84\xA2, please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
				flipc(60);

				char path[512], cfile[512], ffile[512], cfile0[16];
				for(int n=0;n<128;n++){
				sprintf(path, "%s/BDMV/CLIPINF", menu_list[game_sel].path); dir=opendir (path);	while(1) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".clpi")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.CPI", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); rename(ffile, cfile);}}closedir(dir);
				sprintf(path, "%s/BDMV/BACKUP/CLIPINF", menu_list[game_sel].path); dir=opendir (path);	while(1) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".clpi")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.CPI", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); rename(ffile, cfile);}}closedir(dir);

				sprintf(path, "%s/BDMV/PLAYLIST", menu_list[game_sel].path); dir=opendir (path);	while(1) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".mpls")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.MPL", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); rename(ffile, cfile);}}closedir(dir);
				sprintf(path, "%s/BDMV/BACKUP/PLAYLIST", menu_list[game_sel].path); dir=opendir (path);	while(1) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".mpls")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.MPL", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); rename(ffile, cfile);}}closedir(dir);

				sprintf(path, "%s/BDMV/STREAM", menu_list[game_sel].path); dir=opendir (path);	while(1) { struct dirent *entry=readdir (dir);	if(!entry) break; sprintf(cfile0, "%s", entry->d_name); 
				if(strstr (cfile0,".m2ts")!=NULL) {cfile0[5]=0; sprintf(cfile, "%s/%s.MTS", path, cfile0); sprintf(ffile, "%s/%s", path, entry->d_name); rename(ffile, cfile);}}closedir(dir);

				sprintf(path, "%s/BDMV/index.bdmv", menu_list[game_sel].path);	sprintf(cfile, "%s/BDMV/INDEX.BDM", menu_list[game_sel].path); rename(path, cfile);
				sprintf(path, "%s/BDMV/BACKUP/index.bdmv", menu_list[game_sel].path);	sprintf(cfile, "%s/BDMV/BACKUP/INDEX.BDM", menu_list[game_sel].path); rename(path, cfile);

				sprintf(path, "%s/BDMV/MovieObject.bdmv", menu_list[game_sel].path); sprintf(cfile, "%s/BDMV/MOVIEOBJ.BDM", menu_list[game_sel].path); rename(path, cfile);
				sprintf(path, "%s/BDMV/BACKUP/MovieObject.bdmv", menu_list[game_sel].path); sprintf(cfile, "%s/BDMV/BACKUP/MOVIEOBJ.BDM", menu_list[game_sel].path); rename(path, cfile);
				}
				sprintf(menu_list[game_sel].content, "AVCHD");
				cellMsgDialogAbort();
			}
		}


		if(strstr(menu_list[game_sel].content,"BDMV")!=NULL && payload!=0)
		{	
			sprintf(filename, "Activate currently selected Blu-ray\xE2\x84\xA2 (BDMV) video folder?\n\n::: %s :::\n\n[%s: %s]\n\nNote: You can start video playback from XMB\xE2\x84\xA2 [Video] tab", menu_list[game_sel].title, menu_list[game_sel].entry, menu_list[game_sel].details);
			dialog_ret=0;
			ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaab, NULL );
			wait_dialog();
			if(dialog_ret==1)  
			{
				syscall_mount2((char *)"/dev_bdvd", (char *)menu_list[game_sel].path);
				ret = unload_modules();
				sys_process_exit(1); break;
			}
			else goto skip_1;
		} //BDMV

		if(strstr(menu_list[game_sel].content,"AVCHD")!=NULL)
		{
			sprintf(filename, "Activate currently selected AVCHD\xE2\x84\xA2 video folder?\n\n::: %s :::\n\nNote: You can start video playback from XMB\xE2\x84\xA2 [Video] tab", menu_list[game_sel].title); //, menu_list[game_sel].entry, menu_list[game_sel].details
			dialog_ret=0;
			ret = cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaab, NULL );
			wait_dialog();
			if(dialog_ret==1)  
			{


			if(strstr(menu_list[game_sel].path,"/dev_hdd0")!=NULL)  
			{

		dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Activating AVCHD\xE2\x84\xA2 structure, please wait...", dialog_fun2, (void*)0x0000aaab, NULL );
		flipc(60);

		char usb_save[32]="/none"; usb_save[5]=0;


		sprintf(filename, "/dev_sd");
		if(stat(filename, &s3)>=0) {
				sprintf(usb_save, "/dev_sd/PRIVATE");
				 if(stat(usb_save, &s3)<0) mkdir(usb_save, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
		}

		if(stat(usb_save, &s3)<0) {
			sprintf(filename, "/dev_ms");
			if(stat(filename, &s3)>=0) {
					sprintf(usb_save, "/dev_ms");
			}
		}

		if(stat(usb_save, &s3)<0) {
			for(int n=0;n<9;n++){
				sprintf(filename, "/dev_usb00%i", n);
				if(stat(filename, &s3)>=0) {
					sprintf(usb_save, "%s", filename);
					break;
				}
			}
		}

		if(stat(usb_save, &s3)>=0) {

			sprintf(filename, "%s/AVCHD", usb_save); if(stat(filename, &s3)<0) mkdir(filename, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
			sprintf(filename, "%s/AVCHD/BDMV", usb_save); if(stat(filename, &s3)<0) mkdir(filename, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);

			sprintf(filename, "%s/AVCHD/BDMV/INDEX.BDM", usb_save); if(stat(filename, &s3)<0) file_copy((char *) avchdIN, (char *) filename, 0);
			sprintf(filename, "%s/AVCHD/BDMV/MOVIEOBJ.BDM", usb_save);	if(stat(filename, &s3)<0) file_copy((char *) avchdMV, (char *) filename, 0);

			sprintf(filename, "%s/AVCHD", usb_save);
			sprintf(usb_save, "%s", filename);
	
			cellMsgDialogAbort();
			syscall_mount2((char *)usb_save, (char *)menu_list[game_sel].path);

			ret = unload_modules();
			sys_process_exit(1); break;

			}
		else

			{
				dialog_ret=0; cellMsgDialogAbort();
				ret = cellMsgDialogOpen2( type_dialog_ok, "Please attach USB, SDHC or MemoryStick\xE2\x84\xA2 storage device to activate AVCHD\xE2\x84\xA2 HDD playback!", dialog_fun2, (void*)0x0000aaab, NULL );					
				wait_dialog();
				goto skip_1;

			}


			}



				char usb_ROOT[12];
				char avchd_OLD[64];
				char avchd_OLD_path[128];
				char avchd_ROOT[64];
				char avchd_CURRENT[64];
				char title_backup[64];
				char line[64];
				char CrLf[2]; CrLf [0]=13; CrLf [1]=10; CrLf[2]=0;
				FILE *fpA;

				strncpy(usb_ROOT, menu_list[game_sel].path, 11); usb_ROOT[11]=0;

				sprintf(avchd_ROOT, "%s/AVCHD", usb_ROOT);
				sprintf(avchd_CURRENT, "%s", menu_list[game_sel].path);

				sprintf(avchd_OLD, "AVCHD_VIDEO_[%i]", (int)(time(NULL)));
				sprintf(avchd_OLD_path, "%s/%s", usb_ROOT, avchd_OLD);

				if(strcmp(avchd_CURRENT, avchd_ROOT))
				{
					if(stat(avchd_ROOT, &s)>=0) // AVCHD exists and has to be renamed
					{ 
						sprintf(title_backup, "%s/TitleBackup.txt", avchd_ROOT);
						if(stat(title_backup, &s)<0) {
							fpA = fopen ( title_backup, "w" );
							fputs ( avchd_OLD,  fpA );fputs ( CrLf,  fpA );
							fclose(fpA);
						}
						else 
						{
							fpA = fopen ( title_backup, "r" );
							if ( fpA != NULL ) 
							{
								while ( fgets ( line, sizeof line, fpA ) != NULL )
								{
									if(strlen(line)>2) {
										strncpy(avchd_OLD, line, strlen(line)-2);avchd_OLD[strlen(line)-2]=0;
										sprintf(avchd_OLD_path, "%s/%s", usb_ROOT, avchd_OLD);
										break;
									}
								}
								fclose(fpA);
							}
						}

						ret=cellFsRename(avchd_ROOT, avchd_OLD_path);

					}
					// AVCHD doesn't exist and selected folder can be renamed
					
						sprintf(title_backup, "%s/TitleBackup.txt", avchd_CURRENT);
						fpA = fopen ( title_backup, "w" );
						fputs ( menu_list[game_sel].entry,  fpA ); fputs ( CrLf,  fpA );
						fclose(fpA);
						ret=cellFsRename(avchd_CURRENT, avchd_ROOT);

				}

				if(stat(avchd_ROOT, &s)<0) {
					sprintf(line, "Error (%08X) occured while setting active AVCHD folder.\n\nCannot rename [%s] to [%s]", ret, avchd_CURRENT, avchd_ROOT);
					dialog_ret=0;
					ret = cellMsgDialogOpen2( type_dialog_ok, line, dialog_fun2, (void*)0x0000aaab, NULL );
					wait_dialog();
				}
				else
				// Exit to XMB after AVCHD rename
				{
					unload_modules();
					syscall_mount( (char *) "/dev_bdvd", mount_bdvd);
					sys_process_exit(1); break;
				}
			}
			}

		}
		
		else
		{

	   	if(game_sel>=0 && max_menu_list>0 && net_used_ignore())
			{
			int selx=0;
			if((menu_list[game_sel].title[0]=='_' || menu_list[game_sel].split) && payload!=1)
				{
					sprintf(filename, "You cannot launch games with split big files!\n\nTransfer the game to internal HDD and try again\nor use [Hermes] option for BD-Emulator type in SETTINGS XMMB column.");
					if(!menu_list[game_sel].split) { game_last_page=-1; old_fi=-1; };
					menu_list[game_sel].split=1;
					dialog_ret=0;
					ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );
					wait_dialog();
				}
			else
				{

again_sc8:
				reset_mount_points();
				sprintf(filename, "%s/PS3_GAME/PARAM.SFO", menu_list[game_sel].path);
				if(stat(filename, &s3)<0)
					sprintf(filename, "%s/PARAM.SFO", menu_list[game_sel].path);

				char c_split[512]; c_split[0]=0; // /dev_usb001/GAMES/BLES00254-Lego/PS3_GAME/USRDIR/GAME.DAT
				char c_cached[512]; c_cached[0]=0; // /dev_hdd0/game/BLES80608/USRDIR/game_gache/BLES00254/GAME.DAT
				char s_tmp2[512];
				u8 use_cache=0;

				if(stat(filename, &s)>=0)
				{

					if(strstr(filename, "/dev_hdd0/")==NULL && strstr(filename, "/dev_bdvd/")==NULL && (verify_data==2 || (verify_data==1 && payload==1)) )
					{
						abort_copy=0;
						dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Verifying game data, please wait...", dialog_fun2, (void*)0x0000aaab, NULL ); flipc(60);
						sprintf(s_tmp2, "%s/PS3_GAME/USRDIR", menu_list[game_sel].path);
						join_copy= (payload==1 ? 1 : 0);
						max_joined=0;
						global_device_bytes=0;
						num_directories= file_counter= num_files_big= num_files_split= 0;
						time_start= time(NULL);
						my_game_test(s_tmp2, 2); 
						cellMsgDialogAbort(); flip();							

						if( (num_files_big!=0 && payload!=1) || (num_files_big>10) ) {
							sprintf(filename, "You cannot launch games with split big files!\n\nTransfer the game to internal HDD and try again\nor enable [Verify USB Games] option in SETTINGS XMMB column.");
							dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
							if(!menu_list[game_sel].split) { game_last_page=-1; old_fi=-1; };
							menu_list[game_sel].split=1;
							goto cancel_mount2;
						} 

						if(num_files_big<=10 && max_joined<10 && payload==1)
						{
							abort_rec=0;
							//sys8_perm_mode(1);
							fix_perm_recursive(game_cache_dir);
							int reprocess=0;
							for(int sfj=0; sfj<max_joined; sfj++)
							{
								char *p=file_to_join[sfj].split_file;
								sprintf(c_cached, "%s/%s/%s", game_cache_dir, menu_list[game_sel].title_id, p+strlen(menu_list[game_sel].path)+17);
								sprintf(c_split,  "%s", p+strlen(menu_list[game_sel].path));
								sprintf(file_to_join[sfj].cached_file, "%s", c_cached);
								sprintf(file_to_join[sfj].split_file, "%s", c_split);
								if(stat(c_cached, &s2)<0) reprocess=1;
							}


							// check cache
							dialog_ret=0; 
							if(reprocess==1)
							{
								cellMsgDialogOpen2( type_dialog_yes_no, "Pre-processing required for this title.\n\nDo you want to install required data to internal HDD?", dialog_fun1, (void*)0x0000aaaa, NULL );
								wait_dialog();
								if(!menu_list[game_sel].split) { game_last_page=-1; old_fi=-1; };
								menu_list[game_sel].split=1;
								if(dialog_ret!=1) goto cancel_mount2;

								abort_copy=0;
								char s_tmp[512]; sprintf(s_tmp, "%s/%s", game_cache_dir, menu_list[game_sel].title_id);
								mkdir(s_tmp, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(s_tmp, CELL_FS_S_IFDIR | 0777);
								my_game_delete(s_tmp);
								mkdir(s_tmp, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(s_tmp, CELL_FS_S_IFDIR | 0777);
								cellFsGetFreeSize((char*)"/dev_hdd0", &blockSize, &freeSize);
								freeSpace = ( (uint64_t) (blockSize * freeSize) );
								abort_copy=0;
								if((uint64_t)global_device_bytes>(uint64_t)freeSpace && freeSpace!=0) my_game_delete(game_cache_dir);

								cellFsGetFreeSize((char*)"/dev_hdd0", &blockSize, &freeSize);
								freeSpace = ( (uint64_t) (blockSize * freeSize) );
								if((uint64_t)global_device_bytes>(uint64_t)freeSpace && freeSpace!=0)
								{
									sprintf(string1, "Not enough space to complete cache operation! (Available: %.2fMB)\n\nAdditional %.2fMB of free space required!", (double) ((freeSpace)/1048576.00f), (double) ((global_device_bytes-freeSpace)/1048576.00f) );	dialog_ret=0;cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
									goto cancel_mount2;
								}

								sprintf(string1, "%s/%s", game_cache_dir, menu_list[game_sel].title_id);
								mkdir(string1, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(string1, CELL_FS_S_IFDIR | 0777);
								char tb_name[512];
								sprintf(tb_name, "%s/PS3NAME.DAT", string1);
								remove(tb_name);
								FILE *fpA;
								fpA = fopen ( tb_name, "w" );
								if(fpA!=NULL)
								{
									fprintf(fpA, "%.0fGB: %s", (double) ((global_device_bytes)/1073741824.00f), (menu_list[game_sel].title[0]=='_' ? menu_list[game_sel].title+1 :menu_list[game_sel].title));
									fclose(fpA);
								}

								//dialog_ret=0; cellMsgDialogOpen2( type_dialog_yes_no, filename, dialog_fun1, (void*)0x0000aaaa, NULL );	wait_dialog();
								dialog_ret=0; cellMsgDialogOpen2( type_dialog_back, "Setting access permissions, please wait!\n\nThis operation will be performed only once.", dialog_fun2, (void*)0x0000aaab, NULL );	flipc(62);
								sprintf(tb_name, "%s", menu_list[game_sel].path);
								//sys8_perm_mode(1);
								abort_rec=0;
								fix_perm_recursive(tb_name);

								new_pad=0;
								cellMsgDialogAbort();
								flipc(62);

								time_start= time(NULL);
								abort_copy=0;
								initConsole();
								file_counter=0;
								copy_mode=0;
								max_joined=0;

								char s_source[512];
								char s_destination[512];
								sprintf(s_source, "%s/PS3_GAME/USRDIR", menu_list[game_sel].path);
								sprintf(s_destination, "%s/%s", game_cache_dir, menu_list[game_sel].title_id);
								join_copy=1;
								my_game_copy((char*)s_source, (char*)s_destination); 
								cellMsgDialogAbort();
								termConsole();
								join_copy=0;
								new_pad=0;
								if(abort_copy)
								{
									sprintf(s_destination, "%s/%s", game_cache_dir, menu_list[game_sel].title_id);
									abort_copy=0;
									my_game_delete(s_destination);
									rmdir(s_destination);
									sprintf(filename, "Operation failed or canceled.");
									dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
									max_joined=0;
									goto cancel_mount2;
								}
								sprintf(s_destination, "%s", game_cache_dir); //, menu_list[game_sel].title_id
								cellFsChmod(s_destination, CELL_FS_S_IFDIR | 0777);
								abort_rec=0;
								fix_perm_recursive(s_destination);
								goto again_sc8;
							}
							use_cache=1;
							//dialog_ret=0; cellMsgDialogOpen2( type_dialog_yes_back, file_to_join, dialog_fun1, (void*)0x0000aaaa, NULL );wait_dialog();
							//goto cancel_mount2;
						}
					}

					if ((menu_list[game_sel].flags & 1)
						&& (stat(menu_list[game_sel].path, &s) < 0
							|| (s.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != (S_IRWXU | S_IRWXG | S_IRWXO))) 
					{

						dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Setting access permissions, please wait...", dialog_fun2, (void*)0x0000aaab, NULL );	flipc(60);
						sprintf(string1, "%s", menu_list[game_sel].path);
						abort_rec=0;
						fix_perm_recursive(string1);
						cellMsgDialogAbort(); flip();							
					}

					if( ( (payload==0 && sc36_path_patch==0) || (menu_list[game_sel].user & IS_DISC) ) && stat((char*)"/dev_bdvd", &s3)<0)
					{
						dialog_ret=0;
						cellMsgDialogOpen2( type_dialog_yes_back, "Please insert an original PLAYSTATION\xC2\xAE\x33 game disc before proceeding!", dialog_fun1, (void*)0x0000aaaa, NULL );
						wait_dialog();
						if(dialog_ret==3 || stat((char*)"/dev_bdvd", &s3)<0) goto cancel_exit_2;
					}
					else
						if(payload==0 && sc36_path_patch==1 && stat((char*)"/dev_bdvd", &s3)<0)
						{
							//dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, "Start your game from [* /app_home] menu.\n\nShould you run into problems - insert an original Playstation(R)3 game disc next time!", dialog_fun2, (void*)0x0000aaab, NULL ); wait_dialog();
							poke_sc36_path( (char *) "/app_home" );
						}

				selx=0;
				get_game_flags(game_sel);
				FILE *fpA;
				//if(strstr(menu_list[game_sel].path,"/dev_hdd0/game/")!=NULL) direct_launch_forced=1;

				if(strstr(menu_list[game_sel].path, "/dev_usb")!=NULL && ( c_firmware==3.15f || c_firmware==3.41f || c_firmware==3.55f) && ((direct_launch_forced==1 && !(menu_list[game_sel].user & IS_DBOOT)) || (menu_list[game_sel].user & IS_BDMIRROR)) )
					{

					if(c_firmware!=3.55f && c_firmware!=3.41f && c_firmware!=3.15f)
					{
						dialog_ret=0; cellMsgDialogOpen2( type_dialog_ok, "Your current configuration doesn't support this function!", dialog_fun2, (void*)0x0000aaab, NULL );	wait_dialog();
						goto cancel_mount2;
					}

					ret = mod_mount_table((char *) "restore", 0); //restore
					if(stat((char*)"/dev_bdvd/PS3_GAME/PARAM.SFO", &s3)<0 && (menu_list[game_sel].user & IS_DISC))
					{
						dialog_ret=0;
						cellMsgDialogOpen2( type_dialog_yes_back, "Please insert an original PLAYSTATION\xC2\xAE\x33 game disc before proceeding!", dialog_fun1, (void*)0x0000aaaa, NULL );
						wait_dialog();
						if(dialog_ret==3 || stat((char*)"/dev_bdvd", &s3)<0) goto cancel_exit_2;
					}

					selx=1;
					char just_drive[32];
					char usb_mount0[512], usb_mount1[512], usb_mount2[512];
					char path_backup[512], path_bup[512];
				
					/* // test
					sprintf(fileboot, "%s", menu_list[game_sel].path);
					syscall_mount(fileboot, mount_bdvd);
					ret = mod_mount_table(just_drive, 1); //modify
					unload_modules(); sys_process_exit(1); */

					ret = mod_mount_table((char *) "restore", 0); //restore
					if(ret)
					{

						strncpy(just_drive, menu_list[game_sel].path, 11); just_drive[11]=0;
						sprintf(filename, "%s/PS3_GAME/PARAM.SFO", menu_list[game_sel].path);
						change_param_sfo_version(filename);

						sprintf(usb_mount1, "%s/PS3_GAME", just_drive);

						if(stat(usb_mount1, &s3)>=0) 
						{
							//restore PS3_GAME back to USB game folder
							sprintf(path_bup, "%s/PS3PATH.BUP", usb_mount1);
							if(stat(path_bup, &s3)>=0) {
								fpA = fopen ( path_bup, "r" );
								if(fgets ( usb_mount2, 512, fpA )==NULL) sprintf(usb_mount2, "%s/PS3_GAME_OLD", just_drive);
								fclose(fpA);
								strncpy(usb_mount2, just_drive, 11); //always use current device

							}
							else
								sprintf(usb_mount2, "%s/PS3_GAME_OLD", just_drive);

								int pl, n; char tempname[512];
								pl=strlen(usb_mount2);
								for(n=0;n<pl;n++)
								{
									tempname[n]=usb_mount2[n];
									tempname[n+1]=0;
									if(usb_mount2[n]==0x2F && stat(tempname, &s3)<0) 
									{	
										mkdir(tempname, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR); cellFsChmod(tempname, 0777);
									}
								}


							if(stat(usb_mount2, &s3)<0) rename (usb_mount1, usb_mount2);

						}

						if(stat(usb_mount1, &s3)<0) 
						{

							sprintf(usb_mount0, "%s/PS3_GAME", menu_list[game_sel].path);
							sprintf(path_backup, "%s/PS3PATH.BUP", usb_mount0);
							remove(path_backup);
							fpA = fopen ( path_backup, "w" );
							fputs ( usb_mount0,  fpA );
							fclose(fpA);

							menu_list[game_sel].user|=IS_BDMIRROR;
							set_game_flags(game_sel);

							rename (usb_mount0, usb_mount1);

/*
//dev_bdvd
pokeq(0x80000000002D8608ULL, 0x642F5053335F4741ULL );
pokeq(0x80000000002D8610ULL, 0x4D452F5553524449ULL );
pokeq(0x80000000002D8618ULL, 0x522F47414D452E44ULL );
pokeq(0x80000000002D8620ULL, 0x4154000000000000ULL );

pokeq(0x80000000002D8498ULL, 0x38A0001A4BD7623DULL ); // 09 symbols search
pokeq(0x80000000002D84C0ULL, 0x389D00094BD761C1ULL ); // 09 symbols replace

//app_home

pokeq(0x80000000002D8700ULL, 0x2F6465765F626476ULL );
pokeq(0x80000000002D8708ULL, 0x642F5053335F4741ULL );
pokeq(0x80000000002D8710ULL, 0x4D452F5553524449ULL );
pokeq(0x80000000002D8718ULL, 0x522F47414D452E44ULL );
pokeq(0x80000000002D8720ULL, 0x4154000000000000ULL );

pokeq(0x80000000002D8500ULL, 0x6084870038A00022ULL ); // 0x002D7800 (/app_home) 9 search
pokeq(0x80000000002D8528ULL, 0xE87F0008389D0009ULL ); // 9 replace

system_call_1(36, (uint32_t) "/dev_hdd0/cache/BLES00XXX");
*/
							ret = mod_mount_table(just_drive, 1); //modify

							if(ret)
							{
								if(use_cache) mount_with_cache(menu_list[game_sel].path, max_joined, menu_list[game_sel].user, menu_list[game_sel].title_id);
								unload_modules(); sys_process_exit(1);
							}
							else
							{
								mod_mount_table((char *) "reset", 0); //reset
								rename (usb_mount1, usb_mount0);
								dialog_ret=0; ret = cellMsgDialogOpen2( type_dialog_ok, "Error occured while parsing device mount table!", dialog_fun2, (void*)0x0000aaab, NULL );; wait_dialog();
								rename (usb_mount1, usb_mount0);
								goto cancel_mount2;
							}


						}
						else
						{
							dialog_ret=0;
							mod_mount_table((char *) "reset", 0); //reset
							ret = cellMsgDialogOpen2( type_dialog_ok, "Error occured while moving game to new location!", dialog_fun2, (void*)0x0000aaab, NULL );					
							wait_dialog();
						}


					}
					else
					{
						dialog_ret=0;
						ret = cellMsgDialogOpen2( type_dialog_ok, "Error occured while parsing device mount table!", dialog_fun2, (void*)0x0000aaab, NULL );					
						wait_dialog();
					}

cancel_mount2:
					use_cache=0;
					join_copy=0;
					new_pad=0;
					dialog_ret=0;
					goto skip_1;

					}

					sprintf(filename, "%s/USRDIR/RELOAD.SELF", menu_list[game_sel].path);
					if(stat(filename, &s3)>=0)
					{
						unload_modules();
						sys_game_process_exitspawn2((char *) filename, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
					}

					if(strstr(menu_list[game_sel].path, "/dev_usb")==NULL && ((menu_list[game_sel].user & IS_BDMIRROR) || (menu_list[game_sel].user & IS_USB && !(menu_list[game_sel].user & IS_HDD)) ) ) 
					{	
						dialog_ret=0;
						ret = cellMsgDialogOpen2( type_dialog_ok, "This title cannot be loaded from internal HDD.\n\nTransfer to external USB HDD or change title options.", dialog_fun2, (void*)0x0000aaab, NULL );					
						wait_dialog(); 
						goto cancel_exit_2;
					}

					if(strstr(menu_list[game_sel].path, "/dev_hdd")==NULL && ( (menu_list[game_sel].user & IS_HDD && !(menu_list[game_sel].user & IS_USB)) ) ) 
					{	
						dialog_ret=0;
						ret = cellMsgDialogOpen2( type_dialog_ok, "This title cannot be loaded from external USB HDD.\n\nTransfer to internal HDD or change title options.", dialog_fun2, (void*)0x0000aaab, NULL );					
						wait_dialog(); 
						goto cancel_exit_2;
					}


					sprintf(filename, "%s/PS3_GAME/PARAM.SFO", menu_list[game_sel].path);
					change_param_sfo_version(filename);
					if((menu_list[game_sel].user & IS_PATCHED) && c_firmware==3.41f) pokeq(0x80000000000505d0ULL, memvalnew);
	
					char filename2[1024];
					sprintf(filename2, "%s/PS3_GAME/USRDIR/EBOOT.BIN", menu_list[game_sel].path);

					if((menu_list[game_sel].user & IS_DBOOT) || direct_launch_forced)  
					{
						menu_list[game_sel].user|=IS_DBOOT;
						set_game_flags(game_sel);
						write_last_play( filename2, menu_list[game_sel].path, menu_list[game_sel].title, menu_list[game_sel].title_id, 1);
						unload_modules();

						sprintf(filename2, "%s/PS3_GAME/USRDIR/EBOOT.BIN", menu_list[game_sel].path);
						//if(stat("/dev_bdvd/PS3_GAME/USRDIR/EBOOT.BIN", &s3)>=0) sprintf(filename, "%s", "/dev_bdvd/PS3_GAME/USRDIR/EBOOT.BIN");
						//else if(stat("/app_home/PS3_GAME/USRDIR/EBOOT.BIN", &s3)>=0) sprintf(filename, "%s", "/app_home/PS3_GAME/USRDIR/EBOOT.BIN");
						sprintf(fileboot, "%s", menu_list[game_sel].path);
						if(use_cache) mount_with_cache(menu_list[game_sel].path, max_joined, menu_list[game_sel].user, menu_list[game_sel].title_id);
						else mount_with_ext_data(menu_list[game_sel].path, menu_list[game_sel].user); //syscall_mount(fileboot, mount_bdvd);
						sys_game_process_exitspawn2((char *) filename2, NULL, NULL, NULL, 0, 64, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);

					}
					else
					{
						write_last_play( filename, menu_list[game_sel].path, menu_list[game_sel].title, menu_list[game_sel].title_id, 0);
						unload_modules();
						sprintf(fileboot, "%s", menu_list[game_sel].path);
						if(use_cache) mount_with_cache(menu_list[game_sel].path, max_joined, menu_list[game_sel].user, menu_list[game_sel].title_id);
						else mount_with_ext_data(menu_list[game_sel].path, menu_list[game_sel].user);//syscall_mount(fileboot, mount_bdvd);
						sys_process_exit(1);
					}

				}
				else
				{

					if(strstr(menu_list[game_sel].content,"DVD")!=NULL)
						{

							sprintf(filename, "DVD-Video disc loaded. Insert any DVD-R disc and play the VOB videos from XMB\xE2\x84\xA2 Video tab.\n\n[%s]",menu_list[game_sel].path);

							dialog_ret=0;
							ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );
							wait_dialog();
							syscall_mount( menu_list[game_sel].path, mount_bdvd);
							ret = unload_modules();	exit(0);//sys_process_exit(1);
						}

					if(strstr(menu_list[game_sel].content,"PS2")!=NULL && payload!=0)
						{
						dialog_ret=0; cellMsgDialogOpen2( type_dialog_no, "Loading game disc, please wait...", dialog_fun2, (void*)0x0000aaab, NULL ); flipc(60);

						reset_mount_points();
						uint64_t ret2=0x00ULL;
						ret2 = syscall_838("/dev_bdvd");
						ret2 = syscall_838("/dev_ps2disc");
						ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_UDF", "/dev_ps2disc", 0, 1, 0, 0, 0);
						ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_UDF", "/dev_bdvd", 0, 1, 0, 0, 0);
						syscall_mount( menu_list[game_sel].path, mount_bdvd);

//	ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_SIMPLE", "/dev_ps2disc", 0, 1, 0, 0, 0);
//	ret2 = syscall_837("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_SIMPLE", "/dev_ps2disc1", 0, 1, 0, 0, 0);
//	ret2 = syscall_837("CELL_FS_IOS:PATA0_BDVD_DRIVE", "CELL_FS_UDF", "/dev_bdvd", 0, 1, 0, 0, 0);

						if(payload==1)
						{
							ret=sys8_path_table(0ULL);
							dest_table_addr= 0x80000000007FF000ULL-((sizeof(path_open_table)+15) & ~15);
							open_table.entries[0].compare_addr= ((uint64_t) &open_table.arena[0]) - ((uint64_t) &open_table) + dest_table_addr;
							open_table.entries[0].replace_addr= ((uint64_t) &open_table.arena[0x800])- ((uint64_t) &open_table) + dest_table_addr;
							open_table.entries[1].compare_addr= 0ULL; // the last entry always 0
		
							strncpy(&open_table.arena[0], "/dev_ps2disc", 0x100);    // compare 1
							strncpy(&open_table.arena[0x800], menu_list[game_sel].path, 0x800);     // replace 1
							open_table.entries[0].compare_len= strlen(&open_table.arena[0]);		// 1
							open_table.entries[0].replace_len= strlen(&open_table.arena[0x800]);
							sys8_memcpy(dest_table_addr, (uint64_t) &open_table, sizeof(path_open_table));
							ret=sys8_path_table( dest_table_addr);
						}
					if(payload==2)
						{
							ret=syscall35((char *)"/dev_ps2disc", (char *)menu_list[game_sel].path);
						}

							cellMsgDialogAbort();
							sprintf(filename, "PLAYSTATION\xC2\xAE\x32 game disc loaded!\n\n[%s]\n\n(experimental)",menu_list[game_sel].path);

							dialog_ret=0;
							ret = cellMsgDialogOpen2( type_dialog_ok, filename, dialog_fun2, (void*)0x0000aaab, NULL );
							wait_dialog();

							ret = unload_modules();	sys_process_exit(1);
						}
					else
						{
							if(strstr(menu_list[game_sel].path,"/pvd_usb")!=NULL)
								sprintf(string1, "::: %s :::\n\nYou can't load games from selected device!", menu_list[game_sel].title);
							else
								sprintf(string1, "::: %s :::\n\n%s not found", menu_list[game_sel].title, filename);
	
							dialog_ret=0;
							ret = cellMsgDialogOpen2( type_dialog_ok, string1, dialog_fun2, (void*)0x0000aaab, NULL );
							wait_dialog();
						}
					}
				}
			}
		}
	}

cancel_exit_2:

skip_1:

skip_to_FM:
	if((new_pad & BUTTON_SQUARE) && cover_mode==8) egg=1-egg;

	if ( ( ((old_pad & BUTTON_SQUARE) && cover_mode!=5 && cover_mode!=8) || ((old_pad & BUTTON_TRIANGLE) && cover_mode==8)) && game_sel<max_menu_list && max_menu_list>0 && (cover_mode!=8 || (cover_mode==8 && ( (xmb_icon==6 && xmb[xmb_icon].member[xmb[xmb_icon].first].type==1 && xmb[xmb_icon].size>1) || (xmb_icon==5 && xmb[xmb_icon].member[xmb[xmb_icon].first].type==2) || (xmb_icon==7 && xmb[xmb_icon].size)))) ) {
		//old_pad=0;
		if(cover_mode==4) {	sprintf(auraBG, "%s/AUR5.JPG", app_usrdir); load_texture(text_bmp, auraBG, 1920);}
		int ret_f=open_submenu(text_bmp, &game_sel);
		old_fi=-1;
		if(ret_f) {slide_screen_left(text_FONT);memset(text_bmp, 0, 1920*1080*4);}
		if(cover_mode==8) { {xmb[6].init=0; xmb[7].init=0;}init_xmb_icons(menu_list, max_menu_list, game_sel );}
		if(cover_mode==3) load_texture(text_FONT, userBG, 1920);

		if(ret_f==1 && disable_options!=2 && disable_options!=3) goto copy_title;
		if(ret_f==2 && disable_options!=1 && disable_options!=3) goto delete_title;
		if(ret_f==3) goto rename_title;
		if(ret_f==4) goto update_title;
		if(ret_f==5) goto test_title;
		if(ret_f==6) goto setperm_title;
		if(ret_f==7) goto start_title;

		goto force_reload;
	}

	if ( new_pad & BUTTON_CIRCLE && cover_mode!=5 && cover_mode!=8 && net_used_ignore()) {
		new_pad=0; old_pad=0;
		c_opacity_delta=16;	dimc=0; dim=1;
		dialog_ret=0;
		ret = cellMsgDialogOpen2( type_dialog_yes_no, "Quit to XMB\xE2\x84\xA2 screen?", dialog_fun1, (void*)0x0000aaaa, NULL );
		wait_dialog();
 		
		if(dialog_ret==1){
			reset_mount_points();

	char list_file2[128];
	int i;
	sprintf(list_file2, "%s/LLIST.TXT", app_usrdir);
	FILE *flist; 
	remove(list_file2);
	
	flist = fopen(list_file2, "w");
	sprintf(filename, "%s", "\xEF\xBB\xBF"); fputs (filename,  flist ); 
	for(i=0;i<max_menu_list;i++)
	{
		sprintf(filename, "[%s] %s\r\n", menu_list[i].title_id, menu_list[i].title); fputs (filename,  flist ); 
		sprintf(filename, "        --> %s\r\n",  menu_list[i].path); fputs (filename,  flist ); 
		sprintf(filename, "%s", "\r\n"); fputs (filename,  flist ); 
	}
	fclose(flist);

			unload_modules(); sys_process_exit(1); break;
		}
		//		break; // exit
	
	}

		cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		c_opacity2=c_opacity;
		if(c_opacity2>0xc0) c_opacity2=0xc0;
		if(c_opacity2<0x21) c_opacity2=0x00;

		//mouse pointer
		if(cover_mode==5) {// && c_opacity2>0x20) {
			mouseX+=mouseXD; mouseY+=mouseYD;
			if(mouseX>0.995f) {mouseX=0.995f;mouseXD=0.0f;} if(mouseX<0.0f) {mouseX=0.0f;mouseXD=0.0f;}
			if(mouseY>0.990f) {mouseY=0.990f;mouseYD=0.0f;} if(mouseY<0.0f) {mouseY=0.0f;mouseYD=0.0f;}
//			draw_triangle((mouseX-0.5f)*2.0f, (0.5f-mouseY)*2.0f, 0.02f, 0.035f, -1.0f, 0x2080ffff);
//			draw_triangle((mouseX-0.0011f-0.5f)*2.0f, (0.5f-mouseY+0.0015f)*2.0f, 0.0258f, 0.043f, -1.0f, 0xd0d0d0ff);

		}

//char stick_data[20]=" ";
//sprintf(stick_data,"%i %i X=%f Y=%f", padRXstick, padRYstick, mouseXD, mouseYD);
//cellDbgFontPrintf( 0.5f, 0.03f, 0.7f,0x90909080,stick_data);		

		if(!no_video)
		{
			if(game_sel>=0 && (((mode_list==0) /*&& max_menu_list>0*/)) ) // && png_w!=0 && png_h!=0
				{
				if(cover_mode==0 && max_menu_list>0)
				{
					set_texture( text_bmpS, 320, 320); //ICON0.PNG
					
					if(offX<0 || offY<0 || offX>31 || offY>31 || animation==0 || animation==2) {offX=0; offY=0;}
					if(animation==0 || animation==2) incZ=0;
					{offX=0; offY=0; incZ=0;}
					offY+=(float)(incZ*0.5625f); offX+=incZ; if(offX>30) {incZ=-0.25f;};if(offX<1) {incZ=0.25f;};
					display_img((int)(1440-offX), (int)(80-offY), 320+(int)(offX*2.0f), 176+(int)(offY*2.0f), 320, 176, 0.0f, 320, 320);
				}

				if((cover_mode==1 || cover_mode==6 || cover_mode==7) && max_menu_list>0)
				{
					set_texture( text_bmp, 1920, 1080); //PIC1.PNG
					//incZ=64;
					//offX-=incZ; if(offX<0 || animation==0 || animation==1) {incZ=0; offX=0;}
					//display_img((int)offX, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);

				}

				if(cover_mode==8)
				{
					draw_whole_xmb(0);
				}

				if(cover_mode==2 && max_menu_list>0)
				{
					set_texture( text_bmpS, 320, 320);

					if(offX<0 || offY<0 || offX>31 || animation==0 || animation==2) {offX=0; offY=0;} // offY>31 ||
					if(animation==0 || animation==2) incZ=0;
					if(cover_available==1)
						offY+=(float)(incZ*1.1538f); 
					else
						offY+=(float)(incZ*0.5500f); 
					offX+=incZ; if(offX>30) {incZ=-0.3f;};if(offX<1) {incZ=0.3f;}; 
//					offX=0; offY=0; incZ=0;
					if(cover_available==1)
						display_img((int)(1540-offX), (int)(80-offY), 260+(int)(offX*2.0f), 300+(int)(offY*2.0f), 260, 300, -0.5f, 320, 320);
					else
						display_img((int)(1440-offX), (int)(80-offY), 320+(int)(offX*2.0f), 176+(int)(offY*2.0f), 320, 176, -0.5f, 320, 320);

//					blur_texture(text_bmp, 1920, 1080, 1, 1, 1920, 1080,  0, 0, 1);
					set_texture( text_bmp, 1920, 1080); //PIC1.PNG
					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);

				}				

				if(cover_mode==3 && max_menu_list>0)
				{
					if(cover_available==1) {
					set_texture( text_bmpS, 320, 320); //cover
					
						display_img(1602, 80, 260, 300, 260, 300, 0.0f, 320, 320);
					}

					if(game_bg_overlay==1)
					{
						set_texture( text_bmp, 1920, 1080); //PIC1.PNG
						
						incZ=64;
						offX+=incZ; if(offX>0 || animation==0 || animation==1) {incZ=0; offX=0;}
						display_img((int)(offX), 80, 1547, 844, 1920, 1080, 0.0f, 1920, 1080);

					}

					set_texture( text_FONT, 1920, 1080); //PICBG.PNG
					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);

				}

				if(cover_mode==4 && max_menu_list>0)
				{	

					//if(xmb_sparks) draw_stars();
					set_texture( text_bmpS, 320, 320); //ICON0.PNG
					
					if(offX<0 || offY<0 || offX>31 || offY>36 || animation==0 || animation==2) {offX=0; offY=0;}
					if(animation==0 || animation==2) incZ=0;
					if(cover_available==1)
						offY+=(float)(incZ*1.1538); 
					else
						offY+=(float)(incZ*0.5625); 

					offX+=incZ; if(offX>30) {incZ=-0.25f;};if(offX<1) {incZ=0.5f;};

if(is_sliding==0 || slide_step>19) 
	{
		slideX=0; slideX1=0; slideX2=0; slideX3=0; is_sliding=0; slide_step=0;
	} 
else
	{
		slide_step++;
		slideX1+=(9*is_sliding);
		slideX2+=(13*is_sliding);
		slideX3+=(15*is_sliding);
		slideX+=(18*is_sliding);
		offX=0; offY=0; incZ=0;
	}

	// for COVERS
		//260x300 -> 225x260 : 1.75f x 2.00f
		stepX=(int)(is_sliding*slide_step*1.75f);
		stepY=(int)(is_sliding*slide_step*2.00f);

		//225x260 -> 200x230 : 1.25f x 1.5f
		stepX1=(int)(is_sliding*slide_step*1.25f);
		stepY1=(int)(is_sliding*slide_step*1.50f);

		//200x230 -> 120x138 : 4.00f x 4.6f
		stepX2=(int)(is_sliding*slide_step*4.00f);
		stepY2=(int)(is_sliding*slide_step*4.60f);


	// for ICON0.PNG
		//320x176 -> 240x132 : 4.00f x 2.20f
		istepX=(int)(is_sliding*slide_step*4.00f);
		istepY=(int)(is_sliding*slide_step*2.20f);

		//240x132 -> 200x110 : 2.00f x 1.1f
		istepX1=(int)(is_sliding*slide_step*2.00f);
		istepY1=(int)(is_sliding*slide_step*1.10f);

		//200x110 -> 120x66 : 4.00f x 2.2f
		istepX2=(int)(is_sliding*slide_step*4.00f);
		istepY2=(int)(is_sliding*slide_step*2.20f);




					if(cover_available==1)
						if(is_sliding<0)
							display_img((int)(820-offX-(18*is_sliding*slide_step)), (int)(306-offY+abs(stepY)), 260+(int)(offX*2.0f)-abs(stepX), 300+(int)(offY*2.0f)-abs(stepY), 260, 300, 0.0f, 320, 320);
						else
							display_img((int)(820-offX-(16*is_sliding*slide_step)), (int)(306-offY+abs(stepY)), 260+(int)(offX*2.0f)-abs(stepX), 300+(int)(offY*2.0f)-abs(stepY), 260, 300, 0.0f, 320, 320);
					else
//						display_img((int)(800-offX-slideX), (int)(430-offY), 320+(int)(offX*2.0f), 176+(int)(offY*2.0f), 320, 176, 0.0f);
						if(is_sliding<0)
							display_img((int)(800-offX-(19*is_sliding*slide_step)), (int)(430-offY+abs(istepY)), 320+(int)(offX*2.0f)-abs(istepX), 176+(int)(offY*2.0f)-abs(istepY), 320, 176, 0.0f, 320, 320);
						else
							display_img((int)(800-offX-(15*is_sliding*slide_step)), (int)(430-offY+abs(istepY)), 320+(int)(offX*2.0f)-abs(istepX), 176+(int)(offY*2.0f)-abs(istepY), 320, 176, 0.0f, 320, 320);

					if((game_sel-1)>=0) {
						set_texture( text_CFC_3, 320, 320); //ICON0.PNG
						
						if(cover_available_3==1)
							if(is_sliding<0)
								display_img(500-(16*is_sliding*slide_step), 346+(int)(stepY), 225-stepX, 260-stepY, 260, 300, 0.0f, 320, 320);
							else
								display_img(500-slideX2, 346+(int)(stepY1), 225-stepX1, 260-stepY1, 260, 300, 0.0f, 320, 320);
						else
//							display_img(500-slideX3, 474, 240, 132, 320, 176, 0.0f);
							if(is_sliding<0)
								display_img(500-(15*is_sliding*slide_step), 474+(int)(istepY), 240-istepX, 132-istepY, 320, 176, 0.0f, 320, 320);
							else
								display_img(500-(slideX2), 474+(int)(istepY1), 240-istepX1, 132-istepY1, 320, 176, 0.0f, 320, 320);
					}

					if((game_sel+1)<max_menu_list) {
						set_texture( text_SDC_4, 320, 320); //ICON0.PNG
						
						if(cover_available_4==1)
							if(is_sliding<0)
								display_img(1180-int(slideX3), 346-(int)(stepY1), 225+stepX1, 260+stepY1, 260, 300, 0.0f, 320, 320);
							else
								display_img(1180-int(slideX), 346-(int)(stepY), 225+stepX, 260+stepY, 260, 300, 0.0f, 320, 320);
						else
//							display_img(1180-slideX3, 474, 240, 132, 320, 176, 0.0f);
							if(is_sliding<0)
								display_img(1180-int(slideX3), 474-(int)(istepY1), 240+istepX1, 132+istepY1, 320, 176, 0.0f, 320, 320);
							else
								display_img(1180-int(19*is_sliding*slide_step), 474-(int)(istepY), 240+istepX, 132+istepY, 320, 176, 0.0f, 320, 320);
					}

					if((game_sel+2)<max_menu_list) {
						set_texture( text_MSC_5, 320, 320); //ICON0.PNG
						
						if(cover_available_5==1)
							if(is_sliding<0)
								display_img(1480-slideX2, 376-(int)(stepY2), 200+stepX2, 230+stepY2, 260, 300, 0.0f, 320, 320);
							else
								display_img(1480-slideX3, 376-(int)(stepY1), 200+stepX1, 230+stepY1, 260, 300, 0.0f, 320, 320);
						else
//							display_img(1480-slideX2, 496, 200, 110, 320, 176, 0.0f);
							if(is_sliding<0)
								display_img(1480-slideX2, 496-(int)(istepY2), 200+istepX2, 110+istepY2, 320, 176, 0.0f, 320, 320);
							else
								display_img(1480-slideX3, 496-(int)(istepY1), 200+istepX1, 110+istepY1, 320, 176, 0.0f, 320, 320);
					}

					if((game_sel-2)>=0) {
						set_texture( text_OFF_2, 320, 320); //ICON0.PNG
						
						if(cover_available_2==1)
							if(is_sliding<0)
								display_img(240-slideX2, 376+(int)(stepY1), 200-stepX1, 230-stepY1, 260, 300, 0.0f, 320, 320);
							else
								display_img(240-slideX1, 376+(int)(stepY2), 200-stepX2, 230-stepY2, 260, 300, 0.0f, 320, 320);
						else
//							display_img(240-slideX2, 496, 200, 110, 320, 176, 0.0f);
							if(is_sliding<0)
								display_img(240-slideX2, 496+(int)(istepY1), 200-istepX1, 110-istepY1, 320, 176, 0.0f, 320, 320);
							else
								display_img(240-slideX1, 496+(int)(istepY2), 200-istepX2, 110-istepY2, 320, 176, 0.0f, 320, 320);
					}

					if((game_sel+3)<max_menu_list) {
						set_texture( text_NET_6, 320, 320); //ICON0.PNG
						
						if(cover_available_6==1)
							if(is_sliding<0)
								display_img(1740-slideX1, 468-(int)(stepY2), 120+stepX2, 138+stepY2, 260, 300, 0.0f, 320, 320);
							else
								display_img(1740-slideX2, 468-(int)(stepY2), 120+stepX2, 138+stepY2, 260, 300, 0.0f, 320, 320);
						else
//							display_img(1740-slideX1, 540, 120, 66, 320, 176, 0.0f);
							if(is_sliding<0)
								display_img(1740-slideX1, 540-(int)(istepY2), 120+istepX2, 66+istepY2, 320, 176, 0.0f, 320, 320);
							else
								display_img(1740-slideX2, 540-(int)(istepY2), 120+istepX2, 66+istepY2, 320, 176, 0.0f, 320, 320);
					}

					if((game_sel-3)>=0) {
						set_texture( text_BLU_1, 320, 320); //ICON0.PNG
						
						if(cover_available_1==1)
							display_img(60-slideX1, 468+(int)(stepY2), 120-stepX2, 138-stepY2, 260, 300, 0.0f, 320, 320);
						else
//							display_img(60-slideX1, 540, 120, 66, 320, 176, 0.0f);
							display_img(60-slideX1, 540+(int)(istepY2), 120-istepX2, 66-istepY2, 320, 176, 0.0f, 320, 320);
					}


/*				if(animation==2 || animation==3) {
					BoffX--;
					if(BoffX<= -3840) BoffX=0;

					if(BoffX>= -1920) {
						set_texture( text_bmpUPSR, 1920, 1080); 
						
						display_img((int)BoffX, 0, 1920, 1080, 1280, 720, 0.0f);
					}

					set_texture( text_bmpUPSL, 1920, 1080); 
					
					display_img(1920+(int)BoffX, 0, 1920, 1080, 1280, 720, 0.0f);

					if(BoffX<= -1920) {
						set_texture( text_bmpUPSR, 1920, 1080); 
						
						display_img(3840+(int)BoffX, 0, 1920, 1080, 1280, 720, 0.0f);
					}
				}
				else
					{
					set_texture( text_bmpUPSL, 1920, 1080); 
					
					display_img(0, 0, 1920, 1080, 1280, 720, 0.0f);
					} */

				if(animation==2 || animation==3) {
					BoffX--;
					if(BoffX<= -1920) BoffX=0;

					set_texture( text_bmpUPSR, 1920, 1080); 
					

					if(BoffX>= -1920) {
						display_img((int)BoffX, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					}

					display_img(1920+(int)BoffX, 0, abs((int)BoffX), 1080, abs((int)BoffX), 1080, 0.0f, 1920, 1080);

//					if(BoffX<= -1920) {
//						display_img(3840+(int)BoffX, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
//					}
				}
				else
					{
					set_texture( text_bmpUPSR, 1920, 1080); 
					
					display_img(0, 0, 1920, 1080, 1920, 1080, 0.0f, 1920, 1080);
					} 
				
				}

				if(cover_mode==5) {


/*		if ( (old_pad & BUTTON_LEFT) && mouseX>0.54f ) {mouseX-=0.5f; new_pad=0; old_pad=0;}
		if ( (old_pad & BUTTON_RIGHT) && mouseX<0.5f ) {mouseX+=0.5f; new_pad=0; old_pad=0;}
		if ( (old_pad & BUTTON_UP) && mouseY>0.148f ) {mouseY-=0.026f; new_pad=0; old_pad=0;}
		if ( (old_pad & BUTTON_DOWN) && mouseY<0.9f ) {mouseY+=0.026f; new_pad=0; old_pad=0;} */


		if ( (new_pad & BUTTON_LEFT ) && mouseX>=0.026f ) {mouseX-=0.026f;}
		if ( (new_pad & BUTTON_RIGHT) && mouseX<=0.974f ) {mouseX+=0.026f;}

		if ( ((old_pad & BUTTON_R2) || (old_pad & BUTTON_L2)) && (new_pad & BUTTON_UP)   ) { state_draw=1; if(mouseX<0.54f) first_left=0; else first_right=0; new_pad=0;}//
		if ( ((old_pad & BUTTON_R2) || (old_pad & BUTTON_L2)) && (new_pad & BUTTON_DOWN) ) { state_draw=1; if(mouseX<0.54f) first_left=max_dir_l-20; else first_right=max_dir_r-20; new_pad=0;}//

		if ( ((new_pad & BUTTON_UP) && mouseY>=(0.12f+new_offset) && mouseY<=(0.12f+new_offset+0.026f)) ) { state_draw=1; if(mouseX<0.54f) {first_left-=1; if(first_left>1) {new_pad=0;}} else {first_right-=1; if(first_right>1) {new_pad=0;} } }
		if ( ((new_pad & BUTTON_DOWN) && mouseY>=(0.614f+new_offset) && mouseY<=(0.614f+new_offset+0.026f))  ) { state_draw=1; if(mouseX<0.54f) {first_left+=1; if(first_left+18<max_dir_l){new_pad=0;} } else {first_right+=1; if(first_right+18<max_dir_r){new_pad=0;} }}

		if ( (new_pad & BUTTON_UP	) && mouseY>=0.026f ) {mouseY-=0.026f;}
		if ( (new_pad & BUTTON_DOWN ) && mouseY<=0.974f ) {mouseY+=0.026f;}

		if ( (new_pad & BUTTON_L2) ) { state_draw=1; if(mouseX<0.54f) first_left-=12; else first_right-=12; new_pad=0;}
		if ( (new_pad & BUTTON_R2) ) { state_draw=1; if(mouseX<0.54f) first_left+=12; else first_right+=12; new_pad=0;}

//		if(c_opacity2>0x00)
//			cellDbgFontPrintf( 0.04f+new_offset, 0.029f+new_offset, 1.0f ,COL_HEXVIEW, "Games  |  Update  |  About  |  Help  |  Themes");

		//if(c_opacity2>0x00)
		fm_sel=0x0;
		if(mouseX>=0.035f+new_offset && mouseX<=0.13f+new_offset && mouseY>=0.027f+new_offset && mouseY<=0.068f+new_offset) //games
		{	fm_sel=1;
//			draw_square((0.023f+new_offset-0.5f)*2.0f, (0.5f-0.029f-new_offset)*2.0f, 0.20f, 0.07f, 0.0f, 0x0080ff40);
			if(((new_pad & BUTTON_CROSS) || (new_pad & BUTTON_CIRCLE)))
			{	
				load_texture(text_FMS, legend, 1665);
				load_texture(text_bmpUPSR, playBGR, 1920);
				sprintf(auraBG, "%s/AUR5.JPG", app_usrdir);
				load_texture(text_bmp, auraBG, 1920);
				/*is_sliding=0;
				game_last_page=-1; */
				if ((new_pad & BUTTON_CROSS))
				{
					if(cover_mode==initial_cover_mode)
						cover_mode=0;
					else
						cover_mode=initial_cover_mode;

				}
				else
					cover_mode=4;
				/*
				if(lock_display_mode!=-1) cover_mode=lock_display_mode;
				new_pad=0; old_pad=0; 
				old_fi=-1;
				counter_png=0; */
				cover_mode--; if(cover_mode<0) cover_mode=8;
				goto next_for_FM;
				//goto force_reload;
			}
		}

		//if(c_opacity2>0x00)
		if(mouseX>=0.139f+new_offset && mouseX<=0.245f+new_offset && mouseY>=0.027f+new_offset && mouseY<=0.068f+new_offset) //update
		{
			fm_sel=1<<1;
			//draw_square((0.151f+new_offset-0.5f)*2.0f, (0.5f-0.029f-new_offset)*2.0f, 0.211f, 0.07f, -0.2f, 0x0080ff80);
			if ((new_pad & BUTTON_CROSS))
			{	
				new_pad=0; old_pad=0;
				force_update_check=1;
			}
		}


		//if(c_opacity2>0x00)
		if(mouseX>=0.525f+new_offset && mouseX<=0.610f+new_offset && mouseY>=0.027f+new_offset && mouseY<=0.068f+new_offset) //themes
		{
			fm_sel=1<<4;

//			draw_square((0.526f+new_offset-0.5f)*2.0f, (0.5f-0.029f-new_offset)*2.0f, 0.211f, 0.07f, -0.2f, 0x0080ff80);
			if ((new_pad & BUTTON_CROSS))
			{	
				new_pad=0; old_pad=0;
				sprintf(current_right_pane, "%s", themes_dir);
				state_read=1; state_draw=1; 
			}
		}




/*		if(state_read!=0)
		{
			if(stat(current_left_pane, &s3)<0 && strstr(current_left_pane, "/pvd_usb")==NULL && strstr(current_left_pane, "/ps3_home")==NULL && strstr(current_left_pane, "/net_host")==NULL) {sprintf(current_left_pane,"%s", "/");state_read=1;}
			if(stat(current_right_pane, &s3)<0 && strstr(current_right_pane, "/pvd_usb")==NULL && strstr(current_right_pane, "/ps3_home")==NULL && strstr(current_right_pane, "/net_host")==NULL) {sprintf(current_right_pane,"%s","/");state_read=1;}

			if(state_read==1 || state_read==2)
				read_dir(current_left_pane, pane_l, &max_dir_l); 
			if(state_read==1 || state_read==3)
				read_dir(current_right_pane, pane_r, &max_dir_r);
//			state_read=0;
		}*/

		float pane_x_l=0.04f+new_offset;
		float pane_x_r=0.54f;

		if(first_left>max_dir_l) first_left=0;
		if(first_right>max_dir_r) first_right=0;
		if(first_left<0) first_left=0;
		if(first_right<0) first_right=0;

		int help_open=0, about_open=0;
		if(mouseX>=0.41f+new_offset && mouseX<=0.495f+new_offset && mouseY>=0.027f+new_offset && mouseY<=0.068f+new_offset) 
		{
			help_open=1; //help
			fm_sel=1<<3;
//			draw_square((0.411f+new_offset-0.5f)*2.0f, (0.5f-0.029f-new_offset)*2.0f, 0.17f, 0.07f, -0.2f, 0x0080ff80);
/*			if ((new_pad & BUTTON_CROSS))
			{	
				if(stat(helpMME, &s3)>=0)
				{
					mouseY+=0.08;
					unload_modules();
					sys_game_process_exitspawn2((char*) helpMME, NULL, NULL, NULL, 0, 1001, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);	
				}

			}
*/
		}

		if(mouseX>=0.275f+new_offset && mouseX<=0.38f+new_offset && mouseY>=0.027f+new_offset && mouseY<=0.068f+new_offset) about_open=1; //about

		if(c_opacity2==0x00) {help_open=0; about_open=0;}

		//if(help_open) draw_square((0.411f+new_offset-0.5f)*2.0f, (0.5f-0.029f-new_offset)*2.0f, 0.17f, 0.07f, -0.2f, 0x0080ff80);
		//if(about_open) draw_square((0.282f+new_offset-0.5f)*2.0f, (0.5f-0.029f-new_offset)*2.0f, 0.21f, 0.07f, -0.2f, 0x0080ff80);

		if((help_open || about_open))
			{
				new_pad=0; old_pad=0;
				if(about_open)
				{                  
					fm_sel=1<<2;
					draw_square((0.250f-0.5f)*2.0f, (0.5f-0.210)*2.0f, 1.0f, 1.0f, -0.2f, 0x10101000);
					draw_square((0.240f-0.5f)*2.0f, (0.5f-0.200)*2.0f, 1.04f, 1.04f, -0.2f, 0x00080ff40);
					sprintf(string1, "About multiMAN\n ver %s\n", current_version);
					cellDbgFontPrintf( 0.43f, 0.25f, 0.7f, 0xa0a0a0a0, string1);
					cellDbgFontPrintf( 0.28f, 0.44f, 0.7f, 0xa0c0c0c0, 
				   "   multiMAN is a hobby project, distributed in the\nhope that it will be useful, but WITHOUT ANY\nWARRANTY; without even the implied  warranty of\nMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.");
					cellDbgFontPrintf( 0.28f, 0.64f, 0.7f, 0xa0c0c0c0, 
				   "                Copyleft (c) 2011");
				}

				/*if(help_open && stat(helpNAV, &s3)>=0 && stat(helpMME, &s3)<0) {
					mouseY+=0.08;
					load_texture(text_bmp, helpNAV, 1920);
					int opaq=0x04, oinc=1;
					while(1)
					{
						cellSysutilCheckCallback();
						cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G |	CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
						set_texture( text_bmp, 1920, 1080);
						display_img(0, 0, 1920, 1080, 1920, 1080, -0.15f, 1920, 1080);
						cellDbgFontPrintf( 0.35f, 0.88f, 1.2f, (0x00909090|(opaq<<24)), "Press [O] to close");
						opaq+=oinc; if (opaq>0xc0) oinc=-2; if (opaq<0x02) oinc=2;
						setRenderColor();
						cellDbgFontDrawGcm();
						flip();

						pad_read();
						if ( (new_pad & BUTTON_CIRCLE) || (new_pad & BUTTON_TRIANGLE) ) { new_pad=0; old_pad=0; break;}
					}
					help_open=0;
				} */

				if(help_open)// && stat(helpNAV, &s3)<0 && stat(helpMME, &s3)<0)
				{		
					fm_sel=1<<3;
					draw_square((0.250f-0.5f)*2.0f, (0.5f-0.210)*2.0f, 1.0f, 1.0f, -0.2f, 0x10101000);
					draw_square((0.240f-0.5f)*2.0f, (0.5f-0.200)*2.0f, 1.04f, 1.04f, -0.2f, 0x00080ff40);
					cellDbgFontPrintf( 0.38f, 0.25f, 0.9f, 0xa0a0a0a0, "Navigation and keys");

					if(confirm_with_x)
					{
					cellDbgFontPrintf( 0.28f, 0.300f, 0.7f, 0xa0c0c0c0, 
				   "[D-PAD ] - move mouse in fixed increments\n[STICKS] - move mouse pointer\n\n[O] - Open command menu\n\n[R1] - Switch to next GAMES display mode\n[L1] - Switch to prev GAMES display mode\n[L2] - Page up\n[R2] - Page down");
					cellDbgFontPrintf( 0.28f, 0.52f, 0.7f, 0xa0c0c0c0, 
				   "[X]  - Enter selected folder\n[X]  - View image or play music/video file\n\n[R2]+[UP]   - Scroll to top of file list\n[R2]+[DOWN] - Scroll to bottom of file list");
					cellDbgFontPrintf( 0.28f, 0.64f, 0.7f, 0xa0c0c0c0, 
				   "[X] - Load device folders in left pane\n[O] - Load device folders in right pane"
					);
					}
					else
					{
					cellDbgFontPrintf( 0.28f, 0.300f, 0.7f, 0xa0c0c0c0, 
				   "[D-PAD ] - move mouse in fixed increments\n[STICKS] - move mouse pointer\n\n[X] - Open command menu\n\n[R1] - Switch to next GAMES display mode\n[L1] - Switch to prev GAMES display mode\n[L2] - Page up\n[R2] - Page down");
					cellDbgFontPrintf( 0.28f, 0.52f, 0.7f, 0xa0c0c0c0, 
				   "[O]  - Enter selected folder\n[O]  - View image or play music/video file\n\n[R2]+[UP]   - Scroll to top of file list\n[R2]+[DOWN] - Scroll to bottom of file list");
					cellDbgFontPrintf( 0.28f, 0.64f, 0.7f, 0xa0c0c0c0, 
				   "[O] - Load device folders in left pane\n[X] - Load device folders in right pane"
					);
					}

				} 

			}
//		else
			{

			//if(c_opacity2>0x20 && viewer_open==0)
				{
//reload:
					if(state_read)
					{
						if(stat(current_left_pane, &s3)<0 && strstr(current_left_pane, "/pvd_usb")==NULL && strstr(current_left_pane, "/ps3_home")==NULL && strstr(current_left_pane, "/net_host")==NULL) {sprintf(current_left_pane,"%s", "/");state_read=1; state_draw=1;}
						if(stat(current_right_pane, &s3)<0 && strstr(current_right_pane, "/pvd_usb")==NULL && strstr(current_right_pane, "/ps3_home")==NULL && strstr(current_right_pane, "/net_host")==NULL) {sprintf(current_right_pane,"%s","/");state_read=1; state_draw=1;}

						if(strstr(current_left_pane, "/pvd_usb")!=NULL && !pfs_enabled) {sprintf(current_left_pane,"%s", "/");state_read=1; state_draw=1;}
						if(strstr(current_right_pane, "/pvd_usb")!=NULL && !pfs_enabled) {sprintf(current_right_pane,"%s", "/");state_read=1; state_draw=1;}

						if(state_read==1 || state_read==2) read_dir(current_left_pane, pane_l, &max_dir_l); 
						if(state_read==1 || state_read==3) read_dir(current_right_pane, pane_r, &max_dir_r);
					}

//					if(state_read || state_draw)
					{
						state_read=0;					
						max_ttf_label=0;
						draw_dir_pane( pane_l, max_dir_l, first_left, 22-(int)((1080*new_offset)/18.0f+1), pane_x_l);
						draw_dir_pane( pane_r, max_dir_r, first_right, 22-(int)((1080*new_offset)/18.0f+1), pane_x_r);
						if(state_read==1 || state_read==2) read_dir(current_left_pane, pane_l, &max_dir_l); 
						if(state_read==1 || state_read==3) read_dir(current_right_pane, pane_r, &max_dir_r);
					}

					if(state_draw || state_read)
					{
						if(state_read==1 || state_read==2) read_dir(current_left_pane, pane_l, &max_dir_l); 
						if(state_read==1 || state_read==3) read_dir(current_right_pane, pane_r, &max_dir_r);
						if(state_read)
						{
							max_ttf_label=0;
							draw_dir_pane( pane_l, max_dir_l, first_left, 22-(int)((1080*new_offset)/18.0f+1), pane_x_l);
							draw_dir_pane( pane_r, max_dir_r, first_right, 22-(int)((1080*new_offset)/18.0f+1), pane_x_r);
						}

						//memset(text_bmpUPSR, 0x00, V_WIDTH*V_HEIGHT*4);
						for(int fsr=0; fsr<V_WIDTH*V_HEIGHT*4; fsr+=4) *(uint32_t*) ((uint8_t*)(text_bmpUPSR)+fsr)=0x32323280;

						flush_ttf(text_bmpUPSR, V_WIDTH, V_HEIGHT);

						max_ttf_label=0;
						sprintf(string1,"[%s]", current_left_pane); string1[56]=0x2e; string1[57]=0x2e; string1[58]=0x5d; string1[59]=0;
						print_label( 0.04f+new_offset, 0.085f+new_offset, 0.8f, 0xc0c0c0c0, string1, 1.04f, 0.0f, 17);

						sprintf(string1,"[%s]", current_right_pane); string1[56]=0x2e; string1[57]=0x2e; string1[58]=0x5d; string1[59]=0;
						print_label( 0.54f, 0.085f+new_offset, 0.8f, 0xc0c0c0c0, string1, 1.04f, 0.0f, 17);

						flush_ttf(text_bmpUPSR, V_WIDTH, V_HEIGHT);

						state_draw=0;
					}
					max_ttf_label=0;

				}
				//if(c_opacity2<0x21) { screen_saver(); state_draw=1; c_opacity=0xff; c_opacity2=0xff;}
			    if ( (ss_timer>=(ss_timeout*60) && ss_timeout) ) screen_saver();
			}


//					cellDbgFontPrintf( 0.4f, 0.4f, 1.0f,0xa0a0a0a0,"     OS GUI\n\n(not implemented)");



//		draw_square((mouseX-0.5f)*2.0f, (0.5f-mouseY)*2.0f, 0.02f, 0.035f, 0.0f, 0x2080ffff);
//		draw_square(-1.0f, (0.5f - 0.02f)*2.0f, 2.0f, 0.10f, 0.0f, 0x90909080);
//		draw_square(-1.0f, (0.5f - 0.92f)*2.0f, 2.0f, 0.13f, 0.0f, 0x90909080);

//		draw_fileman();

		if ( (new_pad & BUTTON_CROSS) && mouseX>=0.89459375f && mouseX<=0.9195f && mouseY>=0.05222f && mouseY<=0.09666f && net_used_ignore()) 
		{
		dialog_ret=0; new_pad=0; old_pad=0;
		ret = cellMsgDialogOpen2( type_dialog_yes_no, "Quit to XMB\xE2\x84\xA2 screen?", dialog_fun1, (void*)0x0000aaaa, NULL );
		wait_dialog();
 		
		if(dialog_ret==1)
		{	
			reset_mount_points();			
			unload_modules(); sys_process_exit(1); break;}
			new_pad=0; old_pad=0;
		}

  time ( &rawtime );
  timeinfo = localtime ( &rawtime );

//  DPrintf("\nCurrent local time and date: %s", asctime (timeinfo) );
//  DPrintf("%02d:%02d:%02d (%i) %02d/%02d/%04d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, timeinfo->tm_wday, timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900 );
//  DPrintf("%02d:%02d:%02d (%i) %02d/%02d/%04d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, timeinfo->tm_wday, timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900 );

//		cellDbgFontPrintf( 0.87f, 0.93f, ptsize ,0xc0202020, teststring);
//		cellDbgFontPrintf( 0.05f, 0.025f, 1.3f ,0xc0202020, "File manager");

//		cellDbgFontPrintf( 0.04f, 0.029f, 1.0f ,0xc0a0a0a0, "|  Games  |  Update  |  Help  |  About |");


	float y = 0.83f-0.053f;
	char str[256];


	int ok=0, n=0;
	float len;
	float x=0.07;

	char sizer[255];
	char path[255];

	int xZOOM=0;

	for(n=0;n<17;n++)
		{
		//if(c_opacity2==0x00) break;
		ok=0;
		if(n==11 && hide_bd==1) continue;

		if((fdevices>>n) & 1) ok=1;

		if( ok || n==12 || (n==16 && mount_hdd1==1) )

			{
			switch(n)
				{
				case 0:
					sprintf(str, "HDD");
					sprintf(path, "/dev_hdd0");
					break;

				case 16:
					sprintf(str, "HDD#1");
					sprintf(path, "/dev_hdd1");
					break;

				case 7:
					sprintf(str, "USB#%i", n-5);
					sprintf(path, "/dev_usb00%d", n-1);
					break;

				case 11:
					sprintf(str, "%s", menu_list[0].title);
					sprintf(path, "/dev_bdvd");
					break;

				case 12:
					if(cellNetCtlGetInfo(16, &net_info) < 0 || net_avail<0) {str[0]=0; path[0]=0; break;}
					sprintf(str, "PSX Store");
					sprintf(path, "/web");
					break;

				case 13:
					sprintf(str, "USB disk");
					sprintf(path, "/pvd_usb000");
					break;

				case 14:
					sprintf(str, "SDHC card");
					sprintf(path, "/dev_sd");
					break;

				case 15:
					sprintf(str, "MS card");
					sprintf(path, "/dev_ms");
					break;


				default:
					sprintf(str, "USB#%i", n);
					sprintf(path, "/dev_usb00%d", n-1);
					break;
				}

			len=0.03f*(float)(strlen(str));

			xZOOM=0;
			if(mouseX>=x && mouseX<=x+0.05f && mouseY>=y-0.044f && mouseY<=y+0.05) xZOOM=32;
			if(xZOOM>0)
			{
				draw_text_stroke( (x+0.025f)-(float)((0.0094f*(float)(strlen(str)))/2), y-0.115f, 0.75f, 0xffc0c0c0, str);

				if ( ((old_pad & BUTTON_CROSS) || (old_pad & BUTTON_L3) || (old_pad & BUTTON_CIRCLE) || (old_pad & BUTTON_R3)) && strstr(path, "/web")!=NULL)
				{new_pad=0;old_pad=0; launch_web_browser((char*)"http://www.psxstore.com/");}
				 

				if ( (old_pad & BUTTON_CROSS)) {state_draw=1; state_read=2; sprintf(current_left_pane, "%s", path); new_pad=0; old_pad=0;}
				if ( (old_pad & BUTTON_CIRCLE)) {state_draw=1; state_read=3; sprintf(current_right_pane, "%s", path); new_pad=0; old_pad=0;}

			} 

				if(n!=11){
					if(n!=12 && n!=13) {
					cellFsGetFreeSize(path, &blockSize, &freeSize);
					freeSpace = ( ((uint64_t)blockSize * freeSize));
					sprintf(sizer, "%.2fGB", (double) (freeSpace / 1073741824.00f));
					draw_text_stroke( (x+0.025f)-(float)((0.009f*(float)(strlen(sizer)))/2), y+0.045f, 0.75f, COL_LEGEND, sizer);
					}
					if(n==13) {
						sprintf(sizer, "PFS Drive");
						draw_text_stroke( (x+0.025f)-(float)((0.009f*(float)(strlen(sizer)))/2), y+0.045f, 0.75f, COL_LEGEND, sizer);
					}
				}
				else
				{
					sprintf(sizer, "%s", "BD-ROM");
					if(strstr(menu_list[0].content, "PS3")!=NULL) sprintf(sizer, "%s", "PS3 Disc");
					else if(strstr(menu_list[0].content, "PS2")!=NULL) sprintf(sizer, "%s", "PS2 Disc");
					else if(strstr(menu_list[0].content, "DVD")!=NULL) sprintf(sizer, "%s", "DVD Video");
					else if(strstr(menu_list[0].content, "AVCHD")!=NULL) sprintf(sizer, "%s", "AVCHD");
					else if(strstr(menu_list[0].content, "BDMV")!=NULL)  sprintf(sizer, "%s", "Blu-ray");
					draw_text_stroke( (x+0.025f)-(float)((0.009f*(float)(strlen(sizer)))/2), y+0.045f, 0.75f, COL_LEGEND, sizer);
				}


			switch(n)
				{
				case 0:
					set_texture( text_HDD, 320, 320); 
					display_img((int)(x*1920)-(int)(xZOOM/2), (int) (y*1080)-48-xZOOM, 96+xZOOM, 96+xZOOM, 96, 96, 0.0f, 320, 320);
					break;

				case 16:
					set_texture( text_HDD, 320, 320); 
					display_img((int)(x*1920)-(int)(xZOOM/2), (int) (y*1080)-48-xZOOM, 96+xZOOM, 96+xZOOM, 96, 96, 0.0f, 320, 320);
					break;

				case 7:
					set_texture( text_USB, 320, 320); 
					
					display_img((int)(x*1920)-(int)(xZOOM/2), (int) (y*1080)-48-xZOOM, 96+xZOOM, 96+xZOOM, 96, 96, 0.0f, 320, 320);
					break;

				case 11:
					set_texture( text_BLU_1, 320, 320); 
					
					display_img((int)(x*1920)-(int)(xZOOM/2), (int) (y*1080)-48-xZOOM, 96+xZOOM, 96+xZOOM, 96, 96, 0.0f, 320, 320);
					break;

				case 12:
					if(net_avail < 0) {x-=0.13; str[0]=0; break;}
					else
					{
					sprintf(sizer, "%s", net_info.ip_address);
					draw_text_stroke( (x+0.025f)-(float)((0.008f*(float)(strlen(sizer)))/2), y+0.046f, 0.65f, COL_LEGEND, sizer);

					set_texture( text_NET_6, 320, 320); 
					
					display_img((int)(x*1920)-(int)(xZOOM/2), (int) (y*1080)-48-xZOOM, 96+xZOOM, 96+xZOOM, 96, 96, 0.0f, 320, 320);
					break;
					}

				case 13:
//					set_texture( text_CFC_3, 320, 320); 
					set_texture( text_USB, 320, 320); 
					
					display_img((int)(x*1920)-(int)(xZOOM/2), (int) (y*1080)-48-xZOOM, 96+xZOOM, 96+xZOOM, 96, 96, 0.0f, 320, 320);
					break;

				case 14:
					set_texture( text_SDC_4, 320, 320); 
					
					display_img((int)(x*1920)-(int)(xZOOM/2), (int) (y*1080)-48-xZOOM, 96+xZOOM, 96+xZOOM, 96, 96, 0.0f, 320, 320);
					break;

				case 15:
					set_texture( text_MSC_5, 320, 320); 
					
					display_img((int)(x*1920)-(int)(xZOOM/2), (int) (y*1080)-48-xZOOM, 96+xZOOM, 96+xZOOM, 96, 96, 0.0f, 320, 320);
					break;


				default:
					set_texture( text_USB, 320, 320); 
					
					display_img((int)(x*1920)-(int)(xZOOM/2), (int) (y*1080)-48-xZOOM, 96+xZOOM, 96+xZOOM, 96, 96, 0.0f, 320, 320);
					break; 
				}

			x+=0.13;


		}
	}



//sliding background

				draw_fileman();

		if(date_format==0) sprintf(string1, "%02d/%02d/%04d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
		else if(date_format==1) sprintf(string1, "%02d/%02d/%04d", timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_year+1900);
		else if(date_format==2) sprintf(string1, "%04d/%02d/%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday );

		cellDbgFontPrintf( 0.83f, 0.895f, 0.7f ,COL_HEXVIEW, "%s\n %s:%02d:%02d ", string1, tmhour(timeinfo->tm_hour), timeinfo->tm_min, timeinfo->tm_sec);

			} //end cover_mode==5 fileman
		}


			setRenderColor();

//char stick_data[20]=" ";
//sprintf(stick_data,"%i %i X=%f Y=%f", padRXstick, padRYstick, mouseX, mouseY);
//sprintf(stick_data,"BDRC: %i", BDremote);
//padLYstick, padLXstick
//cellDbgFontPrintf( 0.5f, 0.03f, 0.7f,0x90909080,stick_data);

			//if(c_opacity2>0x00) 
//				cellDbgFontPrintf( 0.99f, 0.98f, 0.5f,0x60606080, payloadT); 
				if(www_running==1) 
					//sprintf(www_info, "%i", ss_timer);
					cellDbgFontPrintf( 0.01f, 0.98f, 0.5f,0x60606080, www_info); 

			if(multiStreamStarted==1 && current_mp3!=0 && max_mp3!=0 && (c_opacity2>0x00)) {
				sprintf(filename, "Playing: %i/%i (vol: %i)", current_mp3, max_mp3, (int) (mp3_volume*100)); // mp3_status,
				cellDbgFontPrintf( 0.82f, 0.98f, 0.5f,0x60606060, filename); 
			}

//				cellDbgFontPrintf( 0.02f, 0.90f, 0.5f,0xc0c0c0c0, mouseInfo); 
//				cellDbgFontPrintf( 0.02f, 0.98f, 0.5f,0xffffffff, mouseInfo2); 

			if(patchmode==1	&& (c_opacity2>0x00)) 
			{
				if(cover_mode<3)
					cellDbgFontPrintf( 0.78f, 0.03f, 0.7f,0x90909080,"  USB patch"); 
				else
					if(cover_mode!=5)
						cellDbgFontPrintf( 0.85f, 0.03f, 0.7f,0x90909080," USB patch");
			}
			
			// square for screen
//			draw_square(-1.0f, 1.0f, 2.0f, 2.0f, 0.9f, 0x101010a0);

			// square for titles
//			if(cover_mode==0)



			if(mode_list==0)
			{
				if(dim==1)
				c_opacity+=c_opacity_delta; 
				if(c_opacity<0x20) c_opacity=0x20;
				if(c_opacity>0xff) c_opacity=0xff;

				if(cover_mode!=8)
				{
					draw_device_list(fdevices | ((game_sel>=0 && max_menu_list>0) ? (menu_list[game_sel].flags<<16) : 0), cover_mode, c_opacity2, menu_list[0].content);
		
					if(game_sel>=0 && max_menu_list>0)
					{
						if(cover_mode==0 || cover_mode==2)
						{
							if(cover_mode==0) 
							draw_square(-0.9f, 0.9f, 2.0f-(0.70f-(0*2)), 2.0-0.50f-(2.0*0), 0.7f, 0x2080ff30);
							draw_list_text( text_bmp, 1920, 1080, menu_list, max_menu_list, game_sel | (0x10000 * ((menu_list[0].flags & 2048)!=0)), dir_mode, display_mode, cover_mode, c_opacity, 0);
							set_texture( text_bmp, 1920, 1080);  display_img(0, 0, 1920, 1080, 1920, 1080, -0.2f, 1920, 1080);


						}
						else
						{
								draw_list( menu_list, max_menu_list, game_sel | (0x10000 * ((menu_list[0].flags & 2048)!=0)), dir_mode, display_mode, cover_mode, game_sel_last, c_opacity);
						}
					}
					else if(cover_mode!=5 && cover_mode!=8) cellDbgFontPrintf( 0.08f, 0.1f, 1.0f, 0x80808080, "Insert Blu-ray game disc...\n\nPress [TRIANGLE] to access system menu.\nEdit options.ini if necessary!\n\nPress [SELECT]+[START] for File Manager mode.");
				}
			}
		}

		cellDbgFontDrawGcm();
		if(cover_mode==5) draw_mouse_pointer(0);

		if(!first_launch) flip();

		//cellSysutilCheckCallback();
	
	}


	reset_mount_points();
	ret = unload_modules();

	return 0;
}

/*static void waitFlip(void)
{
	
	while (cellGcmGetFlipStatus()!=0);// sys_timer_usleep(100);
	cellGcmResetFlipStatus();
}*/

void flip(void)
{
	cellSysutilCheckCallback();

	cellDbgFontPrintf( 0.99f, 0.98f, 0.5f,0x10101010, payloadT); 
	cellDbgFontDrawGcm();
	cellGcmResetFlipStatus();

//	if(max_ttf_label) flush_ttf((uint8_t*)(color_base_addr)+video_buffer*(c_frame_index), V_WIDTH, V_HEIGHT);
	cellGcmSetFlip(gCellGcmCurrentContext, frame_index);

	cellGcmFlush(gCellGcmCurrentContext);
	setDrawEnv();
	setRenderColor();
	cellGcmSetWaitFlip(gCellGcmCurrentContext);

	frame_index = 1- frame_index;// (frame_index+1) & 1;
	setRenderTarget();

	vert_indx=0; // reset the vertex index
	vert_texture_indx=0;

	angle+=3.6f;
	if(angle>=360.f) angle=0.f;
/*	if(cover_mode==8 && !key_repeat)
	{
		if(abs(xmb_slide_y) && abs(xmb_slide_y)<100) sys_timer_usleep( 6673 );
		else if(abs(xmb_slide) && abs(xmb_slide)<100) sys_timer_usleep( 6673 );
	}*/
}

void change_bri(u8 *buffer, int delta, u32 size)
{
	u32 pixel;
	u32 deltaR;
	u32 deltaG;
	u32 deltaB;
//	u32 delta2;
	for(u32 fsr=0; fsr<size; fsr+=4) 
	{
		pixel=*(uint32_t*) ((uint8_t*)(buffer)+fsr);
		deltaR = ((u32)((float)((pixel>>24)&0xff)*(1.0f+(float)delta/100.f)));if(deltaR>0xff) deltaR=0xff;
		deltaG = ((u32)((float)((pixel>>16)&0xff)*(1.0f+(float)delta/100.f)));if(deltaG>0xff) deltaG=0xff;
		deltaB = ((u32)((float)((pixel>> 8)&0xff)*(1.0f+(float)delta/100.f)));if(deltaB>0xff) deltaB=0xff;
		*(uint32_t*) ((uint8_t*)(buffer)+fsr)= deltaR<<24 | deltaG<<16 | deltaB<<8 | pixel&0xff;
	}
}

static void png_thread_entry( uint64_t arg )
{
	(void)arg;
	is_decoding_png=1;
	if(init_finished)
	{
		int _xmb_icon=arg&0xf;
		int cn=arg>>8;
		if(xmb[_xmb_icon].member[cn].status==1)
		{
			load_png_texture_th( xmb[_xmb_icon].member[cn].icon, xmb[_xmb_icon].member[cn].icon_path);//, _xmb_icon==8?408:xmb[_xmb_icon].member[cn].iconw);
			if(png_w_th && png_h_th && ((png_w_th+png_h_th)<=(XMB_THUMB_WIDTH+XMB_THUMB_HEIGHT)) && (xmb[_xmb_icon].member[cn].status==1))
			{
				xmb[_xmb_icon].member[cn].iconw=png_w_th;
				xmb[_xmb_icon].member[cn].iconh=png_h_th;
				xmb[_xmb_icon].member[cn].status=2;
				if(_xmb_icon==7) put_texture_with_alpha_gen( xmb[_xmb_icon].member[cn].icon, xmb_icon_star_small, 32, 32, 32, 320, 283, 5);
			}
			else
			{
				xmb[_xmb_icon].member[cn].status=2;
				if(_xmb_icon==6) xmb[_xmb_icon].member[cn].icon=xmb[6].data;
				else if(_xmb_icon==7) xmb[_xmb_icon].member[cn].icon=xmb[7].data;
				else if(_xmb_icon==5) xmb[_xmb_icon].member[cn].icon=xmb_icon_film;
				else if(_xmb_icon==3) xmb[_xmb_icon].member[cn].icon=xmb_icon_photo;
				else if(_xmb_icon==8) xmb[_xmb_icon].member[cn].icon=xmb_icon_retro;
			}
		}
	}
	is_decoding_png=0;
	sys_ppu_thread_exit(0);
}


static void jpg_thread_entry( uint64_t arg )
{
	(void)arg;
	is_decoding_jpg=1;
	if(init_finished)
	{
		int _xmb_icon=arg&0xf;
		int cn=arg>>8;
		if(xmb[_xmb_icon].member[cn].status==1)
		{
			scale_icon_h=176;
			load_jpg_texture_th( xmb[_xmb_icon].member[cn].icon, xmb[_xmb_icon].member[cn].icon_path, xmb[_xmb_icon].member[cn].iconw);
			if(jpg_w && jpg_h && ((jpg_w+jpg_h)<=(XMB_THUMB_WIDTH+XMB_THUMB_HEIGHT)) && (xmb[_xmb_icon].member[cn].status==1))
			{
				xmb[_xmb_icon].member[cn].iconw=jpg_w;
				xmb[_xmb_icon].member[cn].iconh=jpg_h;
				xmb[_xmb_icon].member[cn].status=2;
			}
			else
			{
				xmb[_xmb_icon].member[cn].status=2;
				if(_xmb_icon==5) xmb[_xmb_icon].member[cn].icon=xmb_icon_film;
				else if(_xmb_icon==3) xmb[_xmb_icon].member[cn].icon=xmb_icon_photo;
				else if(_xmb_icon==8) xmb[_xmb_icon].member[cn].icon=xmb_icon_retro;
			}
		}
	}
	scale_icon_h=0;
	is_decoding_jpg=0;
	sys_ppu_thread_exit(0);
}

void load_jpg_threaded(int _xmb_icon, int cn)
{
	if(is_decoding_jpg) return;
//	while(is_decoding_jpg);
	is_decoding_jpg=1;
	sys_ppu_thread_create( &jpgdec_thr_id, jpg_thread_entry,
						   (_xmb_icon | (cn<<8) ), 
						   70, app_stack_size,
						   0, "multiMAN_jpeg" );//SYS_PPU_THREAD_CREATE_JOINABLE
}

void load_png_threaded(int _xmb_icon, int cn)
{
	if(is_decoding_png) return;
	is_decoding_png=1;
	sys_ppu_thread_create( &pngdec_thr_id, png_thread_entry,
						   (_xmb_icon | (cn<<8) ), 
						   80, app_stack_size,//misc_thr_prio
						   0, "multiMAN_png" );//SYS_PPU_THREAD_CREATE_JOINABLE
}


static void download_thread_entry( uint64_t arg )
{
	(void)arg;
	while(init_finished)
	{
		for(int n=0;n<MAX_DOWN_LIST;n++)
		{
			if(downloads[n].status==1)
			{
				downloads[n].status=2;
				if(stat(downloads[n].local, &s3)<0)
				{
					//sprintf(www_info, "%s", downloads[n].url); 
					download_file_th(downloads[n].url, downloads[n].local, 0);
					//www_info[0]=0;
				}
				sys_timer_usleep(1000*1000);
				//cellSysutilCheckCallback();
			}
		}
		sys_timer_usleep(5000*1000);
//		cellSysutilCheckCallback();
	}
	sys_ppu_thread_exit( 0 );
}



static void misc_thread_entry( uint64_t arg )
{

	(void)arg;
	while(init_finished)
	{

		if(cover_mode==8 && is_game_loading && !drawing_xmb && !first_launch)
		{	
			drawing_xmb=1;
			draw_whole_xmb(xmb_icon);
		}
		else
			sys_timer_usleep(1000*1000);

		cellConsolePoll();
		if(force_mp3)
		{
			main_mp3_th(force_mp3_file);
			force_mp3=false;
		}

	}
	sys_ppu_thread_exit( 0 );
}

