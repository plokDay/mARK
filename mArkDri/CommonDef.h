#pragma once
//#include <minwindef.h>
#define FILE_DEVICE_UNKNOWN             0x00000022
#define METHOD_OUT_DIRECT               2

#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)
#define FUNCTION_FROM_CTL_CODE(ctlCode) (((ctlCode)&0x1FFC)>>2)

#define IOCTL_BASE        0x800
#define MY_CTL_CODE(code)			\
		CTL_CODE					\
		(							\
			FILE_DEVICE_UNKNOWN	,	\
			IOCTL_BASE + (code),	\
			METHOD_OUT_DIRECT,		\
			0						\
		)


typedef	 struct _FIND_FILE_INFO
{
	HANDLE	hFindHandle;

	ULONG dwFileAttributes;
	ULONG createTimeHigh;
	ULONG createTimeLow;
	ULONG64 ftLastAccessTime;
	ULONG writeTimeHigh;
	ULONG writeTimeLow;
	ULONG nFileSizeHigh;
	ULONG nFileSizeLow;
	WCHAR  cFileName[266];
}FIND_FILE_INFO;


typedef struct 
{
	HANDLE hProcess;
	int dwProcessId;
	CHAR     szImageName[100];
	UINT32	  ProcessId;
	UINT32	  ParentProcessId;
	WCHAR     wzFilePath[266];
	UINT_PTR  EProcess;
	int      bUserAccess;
} PROCESS_ENTRY_INFO,*PPROCESS_ENTRY_INFO;
typedef struct _DRIVER_ENTRY_INFORMATION
{
	UINT_PTR  BaseAddress;
	UINT_PTR  Size;
	WCHAR     wzDriverName[100];
	WCHAR     wzDriverPath[266];
} DRIVER_ENTRY_INFO, *PDRIVER_ENTRY_INFO;
typedef struct
{
	UINT16 uIdLimit;
	UINT16 uLowIdtBase;
	UINT16 uHighIdtBase;
}IDT_INFO, *PIDT_INFO;
typedef struct
{
	UINT16 uOffsetLow;
	UINT16 uSeclector;
	UINT8 uReserved;
	UINT8 GetType : 4;
	UINT8 StorageSegment : 1;
	UINT8 DPL : 2;
	UINT8 Present : 1;
	UINT16 uOffsetHigh;
}IDT_ENTRY, *PIDT_ENTRY;
typedef struct
{
	UINT16 uGdLimit;
	UINT16 uLowGdtBase;
	UINT16 uHighGdtBase;
}GDT_INFO, *PGDT_INFO;
typedef struct
{
	UINT64 Limit0_15 : 16;
	UINT64       BaseLow : 16;
	UINT64     BaseMid : 8;
	UINT64 TYPE : 4;
	UINT64 S : 1;
	UINT64 DPL : 2;
	UINT64 P : 1;
	UINT64 Limit16_19 : 4;
	UINT64 AVL : 1;
	UINT64 O : 1;
	UINT64 D_B : 1;
	UINT64 G : 1;
	UINT64 BaseHigh : 8;
}GDT_ENTRY, *PGDT_ENTRY;
typedef struct 
{
	UINT_PTR	BaseAddress;
	UINT_PTR	SizeOfImage;
	WCHAR	    wzFilePath[266];
} MODULE_ENTRY, *PMODULE_ENTRY;
typedef struct
{
	UINT_PTR	Ethread;
	ULONG	TID;
} THREAD_ENTRY, *PTHREAD_ENTRY;
typedef enum _DRIVER_FUNCTION {
	DF_GET_IDT = MY_CTL_CODE(0), // IDT
	DF_GET_FILE_FIRST = MY_CTL_CODE(1), /*扫描文件, 输入一个目录路径(\\??\\D:\\格式) , 函数输出一个查找句柄,和文件信息(FIND_FILE_INFO)*/
	DF_GET_FILE_NEXT = MY_CTL_CODE(2),  /*继续扫描, 输入FIND_FILE_INFO变量,输出文件信息下一个FIND_FILE_INFO,如果没有更多信息,则输出0*/
	DF_GET_PROCESSNUM = MY_CTL_CODE(3),//获得进程的个数
	DF_ENUM_PROCESS = MY_CTL_CODE(4),/*枚举进程*/
	DF_GET_DRIVERNUM = MY_CTL_CODE(5),/*枚举进程*/
	DF_ENUM_DRIVER = MY_CTL_CODE(6),/*枚举驱动*/
	DF_HIDE_DRIVER = MY_CTL_CODE(7),/*隐藏驱动*/
	DF_GET_IDTFIRST = MY_CTL_CODE(8),/*IDT*/
	DF_GET_GDTFIRST = MY_CTL_CODE(9),/*GDT*/
	DF_GET_GDT = MY_CTL_CODE(10),/*GDT*/
	DF_ENUM_SSDT = MY_CTL_CODE(11),/*SSDT*/
	DF_GET_SSDT_COUNT = MY_CTL_CODE(12),/*SSDT*/
	DF_DELETE_FILE = MY_CTL_CODE(13),/*删除文件*/
	DF_PROTECT_PRPCESS = MY_CTL_CODE(14),/*进程保护*/
	DF_TERMINATE_PRPCESS = MY_CTL_CODE(15),/*结束进程*/
	DF_ENUM_MODULEFIRST = MY_CTL_CODE(16),/*结束进程*/
	DF_ENUM_MODULENEXT = MY_CTL_CODE(17),/*结束进程*/
	DF_ENUM_THREADFIRST = MY_CTL_CODE(18),/*结束进程*/
	DF_ENUM_THREADNEXT = MY_CTL_CODE(19),/*结束进程*/
	DF_HIDE_PROCESS = MY_CTL_CODE(20),/*结束进程*/
	DF_KERNEL_RELOAD = MY_CTL_CODE(21)/*内核重载*/
}DRIVER_FUNCTION;
