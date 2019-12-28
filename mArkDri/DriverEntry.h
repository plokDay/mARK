#pragma once
#include <ntifs.h>

typedef struct _PEB_LDR_DATA {
	ULONG                   Length;
	BOOLEAN                 Initialized;
	PVOID                   SsHandle;
	LIST_ENTRY              InLoadOrderModuleList;
	LIST_ENTRY              InMemoryOrderModuleList;
	LIST_ENTRY              InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

//// 下面的结构体是PEB部分字段.

struct _PEB
{
	UCHAR InheritedAddressSpace;                                            //0x0
	UCHAR ReadImageFileExecOptions;                                         //0x1
	UCHAR BeingDebugged;
	UCHAR BitField;
	void* Mutant;                                                           //0x4
	void* ImageBaseAddress;                                                 //0x8
	struct _PEB_LDR_DATA* Ldr;
};
typedef struct _LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY32 InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	ULONG DllBase;
	ULONG EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STRING32 FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT TlsIndex;
	union {
		LIST_ENTRY32 HashLinks;
		struct {
			ULONG SectionPointer;
			ULONG  CheckSum;
		};
	};
	union {
		struct {
			ULONG  TimeDateStamp;
		};
		struct {
			ULONG LoadedImports;
		};
	};
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;
NTSTATUS DriverEntry(DRIVER_OBJECT* driver, UNICODE_STRING* path);
void  Unload(DRIVER_OBJECT* object);
NTSTATUS  Create(DEVICE_OBJECT* device, IRP* irp);
NTSTATUS  Close(DEVICE_OBJECT* device, IRP* irp);

NTSTATUS  DeviceCtrl(DEVICE_OBJECT* device, IRP* irp);

PPEB PsGetProcessPeb(IN PEPROCESS Process);
